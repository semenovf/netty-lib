////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.26 Initial version.
//      2023.01.06 Renamed to `udt_socket` and refactored.
//      2024.07.29 Refactored `udt_socket`.
////////////////////////////////////////////////////////////////////////////////
#include "newlib/udt.hpp"
#include "pfs/assert.hpp"
#include "pfs/endian.hpp"
#include "pfs/i18n.hpp"
#include "pfs/numeric_cast.hpp"
#include "pfs/optional.hpp"
#include "pfs/netty/udt/udt_socket.hpp"
#include <cerrno>

#if _MSC_VER
#   include <winsock2.h>
#else
#   include <netinet/in.h>
#endif

#include "debug_CCC.hpp"

#include "pfs/log.hpp"

static constexpr char const * TAG = "UDT";

namespace netty {
namespace udt {

// ping -c 50
//              avg RTT (ms) | exp_max_counter | exp_threshold (ms)
// -----------------------------------------------------------------------------
// Home Wi-Fi   1.499        | 2               | 625

// int basic_socket::default_exp_counter ()
// {
//     return 2;
// }
//
// std::chrono::milliseconds basic_socket::default_exp_threshold ()
// {
//     return std::chrono::milliseconds{625};
// }

static constexpr int kDefaultExpMaxCounter = 2;
static std::chrono::milliseconds const kDefaultExpThreshold {625};

void udt_socket::init (int mtu, int exp_max_counter, std::chrono::milliseconds exp_threshold, error * perr)
{
    int ai_family   = AF_INET;     // AF_INET | AF_INET6
    int ai_socktype = SOCK_STREAM; // SOCK_DGRAM | SOCK_STREAM
    int ai_protocol = 0;

    socket_id sock = UDT::socket(ai_family, ai_socktype, ai_protocol);

    if (sock == UDT::INVALID_SOCK) {
        pfs::throw_or(perr, error {
            tr::f_("create UDT socket failure: {}", UDT::getlasterror_desc())
        });

        return;
    }

    bool true_value  = true;
    bool false_value = false;

    // UDT Options
    auto rc = UDT::setsockopt(sock, 0, UDT_REUSEADDR, & true_value, sizeof(bool));

    if (rc != UDT::ERROR)
        rc = UDT::setsockopt(sock, 0, UDT_SNDSYN, & false_value, sizeof(bool)); // sending is non-blocking

    if (rc != UDT::ERROR)
        rc = UDT::setsockopt(sock, 0, UDT_RCVSYN, & false_value, sizeof(bool)); // receiving is non-blocking

    if (rc != UDT::ERROR) {
        int mtu_value = mtu;
        rc = UDT::setsockopt(sock, 0, UDT_MSS, & mtu_value, sizeof(mtu_value));
    }

    // Set to UDT default value.
    // Default is 8192 * 1500(MSS) = 12288000
    if (rc != UDT::ERROR) {
        int bufsz = 12288000;
        rc = UDT::setsockopt(sock, 0, UDT_SNDBUF, & bufsz, sizeof(bufsz));
    }

    // Set to UDT default value.
    // Default is 8192 * 1500(MSS) = 12288000
    if (rc != UDT::ERROR) {
        int bufsz = 12288000;
        rc = UDT::setsockopt(sock, 0, UDP_RCVBUF, & bufsz, sizeof(bufsz));
    }

    if (rc != UDT::ERROR) {
        if (exp_max_counter < 0) {
            pfs::throw_or(perr, error {
                  std::make_error_code(std::errc::invalid_argument)
                , tr::f_("bad `exp_max_counter` value: {}", UDT::getlasterror_desc())
            });

            UDT::close(sock);
            return;
        }

        if (exp_threshold < std::chrono::milliseconds{0}) {
            pfs::throw_or(perr, error {
                  std::make_error_code(std::errc::invalid_argument)
                , tr::f_("bad `exp_threshold` value: {}", UDT::getlasterror_desc())
            });

            UDT::close(sock);
            return;
        }

        std::uint64_t exp_threshold_usecs = static_cast<std::uint64_t>(exp_threshold.count()) * 1000;

        rc = UDT::setsockopt(sock, 0, UDT_EXP_MAX_COUNTER, & exp_max_counter, sizeof(exp_max_counter));

        if (rc != UDT::ERROR)
            rc = UDT::setsockopt(sock, 0, UDT_EXP_THRESHOLD, & exp_threshold_usecs, sizeof(exp_threshold_usecs));

    }

    //UDT::setsockopt(sock, 0, UDT_CC, new CCCFactory<debug_CCC>, sizeof(CCCFactory<debug_CCC>));

    if (rc == UDT::ERROR) {
        UDT::close(sock);

        pfs::throw_or(perr, error {
            tr::f_("UDT set socket option failure: {}", UDT::getlasterror_desc())
        });

        return;
    }

    _socket = sock;
}

udt_socket::udt_socket ()
{}

// udt_socket::udt_socket ()
// {
//     init(1500, kDefaultExpMaxCounter, kDefaultExpThreshold, nullptr);
// }

udt_socket::udt_socket (socket_id sock, socket4_addr const & saddr)
    : _socket(sock)
    , _saddr(saddr)
{}

udt_socket::udt_socket (int mtu, int exp_max_counter, std::chrono::milliseconds exp_threshold, error * perr)
{
    init(mtu, exp_max_counter, exp_threshold, perr);
}

udt_socket::udt_socket (int mtu, error * perr)
    : udt_socket(mtu, kDefaultExpMaxCounter, kDefaultExpThreshold, perr)
{}

udt_socket::udt_socket (udt_socket && other) noexcept
{
    this->operator = (std::move(other));
}

udt_socket & udt_socket::operator = (udt_socket && other) noexcept
{
    this->~udt_socket();
    _socket = other._socket;
    _saddr  = other._saddr;
    other._socket = kINVALID_SOCKET;
    return *this;
}

udt_socket::~udt_socket ()
{
    if (_socket >= 0) {
        UDT::close(_socket);
        _socket = kINVALID_SOCKET;
    }
}

udt_socket::operator bool () const noexcept
{
    return _socket != kINVALID_SOCKET;
}

udt_socket::socket_id udt_socket::id () const noexcept
{
    return _socket;
}

socket4_addr udt_socket::saddr () const noexcept
{
    return _saddr;
}

static std::string state_string (int state)
{
    switch (state) {
        case INIT      : return "INIT";
        case OPENED    : return "OPENED";
        case LISTENING : return "LISTENING";
        case CONNECTING: return "CONNECTING";
        case CONNECTED : return "CONNECTED";
        case BROKEN    : return "BROKEN";
        case CLOSING   : return "CLOSING";
        case CLOSED    : return "CLOSED";
        case NONEXIST  : return "NONEXIST";
    }

    return std::string{"<INVALID STATE>"};
}

void udt_socket::dump_options (std::vector<std::pair<std::string, std::string>> & out) const
{
    int opt_size {0};

    // UDT_MSS - Maximum packet size (bytes). Including all UDT, UDP, and IP
    // headers. Default 1500 bytes.
    int mss {0};
    PFS__ASSERT(0 == UDT::getsockopt(_socket, 0, UDT_MSS, & mss, & opt_size), "");
    out.push_back(std::make_pair("UDT_MSS", std::to_string(mss)
        + ' ' + "bytes (max packet size)"));

    // UDT_SNDSYN - Synchronization mode of data sending. True for blocking
    // sending; false for non-blocking sending. Default true.
    bool sndsyn {false};
    PFS__ASSERT(0 == UDT::getsockopt(_socket, 0, UDT_SNDSYN, & sndsyn, & opt_size), "");
    out.push_back(std::make_pair("UDT_SNDSYN", sndsyn
        ? "TRUE (sending blocking)"
        : "FALSE (sending non-blocking)"));

    // UDT_RCVSYN - Synchronization mode for receiving.	true for blocking
    // receiving; false for non-blocking receiving. Default true.
    bool rcvsyn {false};
    PFS__ASSERT(0 == UDT::getsockopt(_socket, 0, UDT_RCVSYN, & rcvsyn, & opt_size), "");
    out.push_back(std::make_pair("UDT_RCVSYN", rcvsyn
        ? "TRUE (receiving blocking)"
        : "FALSE (receiving non-blocking)"));

    // UDT_FC - Maximum window size (packets). Default 25600.
    int fc {0};
    PFS__ASSERT(0 == UDT::getsockopt(_socket, 0, UDT_FC, & fc, & opt_size), "");
    out.push_back(std::make_pair("UDT_FC", std::to_string(fc)
        + ' ' + "packets (max window size)"));

    // UDT_STATE - Current status of the UDT socket.
    std::int32_t state {0};
    PFS__ASSERT(0 == UDT::getsockopt(_socket, 0, UDT_STATE, & state, & opt_size), "");
    out.push_back(std::make_pair("UDT_STATE", state_string(state)));

    // UDT_LINGER - Linger time on close().
    linger lng {0, 0};
    int linger_sz = sizeof(linger);
    PFS__ASSERT(0 == UDT::getsockopt(_socket, 0, UDT_LINGER, & lng, & linger_sz), "");
    out.push_back(std::make_pair("UDT_LINGER"
        , "{" + std::to_string(lng.l_onoff)
        + ", " + std::to_string(lng.l_linger) + "}"));

    // UDT_EVENT - The EPOLL events available to this socket.
    std::int32_t event {0};
    PFS__ASSERT(0 == UDT::getsockopt(_socket, 0, UDT_EVENT, & event, & opt_size), "");

    std::string event_str;

    if (event & UDT_EPOLL_IN)
        event_str += (event_str.empty() ? std::string{} : " | ") + "UDT_EPOLL_IN";
    if (event & UDT_EPOLL_OUT)
        event_str += (event_str.empty() ? std::string{} : " | ") + "UDT_EPOLL_OUT";
    if (event & UDT_EPOLL_ERR)
        event_str += (event_str.empty() ? std::string{} : " | ") + "UDT_EPOLL_ERR";

    out.push_back(std::make_pair("UDT_EVENT", event_str.empty() ? "<empty>" : event_str));
}

// int udt_socket::available (error * perr) const
// {
//     // NOTE: UDT::getsockopt(..., UDT_RCVDATA,...) returns expected result only for STREAM UDT socket.
//     //
//     std::int32_t n {0};
//     int opt_size {sizeof(n)};
//     auto rc = UDT::getsockopt(_socket, 0, UDT_RCVDATA, & n, & opt_size);
//
//     if (rc != 0) {
//         pfs::throw_or(perr, error {
//               errc::socket_error
//             , tr::_("read available data size from socket failure")
//             , UDT::getlasterror_desc()
//         });
//
//         return -1;
//     }
//
//     return n;
// }

int udt_socket::recv (char * data, int len, error * perr)
{
    auto rc = UDT::recv(_socket, data, len, 0);

    if (rc == UDT::ERROR) {
        auto ecode = UDT::getlasterror_code();

        // Error code: 6002
        // Error desc: Non-blocking call failure: no data available for reading.
        if (CUDTException{6, 2, 0}.getErrorCode() == ecode) {
            rc = 0;
        } else {
            LOGE(TAG, "RECV: code={}, text={} (FIXME handle error)", ecode, UDT::getlasterror_desc());

            pfs::throw_or(perr, error {UDT::getlasterror_desc()});
        }
    }

    return rc;
}

send_result udt_socket::send (char const * data, int len, error * /*perr*/)
{
    // int ttl_millis = -1;
    // bool inorder = true;
    //
    // Return
    // > 0             - on success;
    // = 0             - if UDT_SNDTIMEO > 0 and the message cannot be sent
    //                   before the timer expires;
    // -1              - on error
    // auto rc = UDT::sendmsg(_socket, data, len, ttl_millis, inorder);

    auto rc = UDT::send(_socket, data, len, 0);

    if (rc == UDT::ERROR) {
        auto ecode = UDT::getlasterror_code();
        LOGE(TAG, "SEND: code={}, text={} (FIXME handle error)", ecode, UDT::getlasterror_desc());

        // Error code: 6001
        // Error desc: Non-blocking call failure: no buffer available for sending.
        // if (CUDTException{6, 1, 0}.getErrorCode() == ecode)
        return send_result{send_status::failure, 0};
    }

    return send_result{send_status::good, static_cast<decltype(send_result::n)>(rc)};
}

conn_status udt_socket::connect (socket4_addr const & saddr, error * perr)
{
    sockaddr_in addr_in4;

    memset(& addr_in4, 0, sizeof(addr_in4));

    addr_in4.sin_family      = AF_INET;
    addr_in4.sin_port        = pfs::to_network_order(static_cast<std::uint16_t>(saddr.port));
    addr_in4.sin_addr.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(saddr.addr));

    auto rc = UDT::connect(_socket
        , reinterpret_cast<sockaddr *>(& addr_in4)
        , sizeof(addr_in4));

    if (rc == UDT::ERROR) {
        pfs::throw_or(perr, error {tr::f_("socket connect error: {}", UDT::getlasterror_desc())});
        return conn_status::failure;
    }

    _saddr = saddr;

    auto status = UDT::getsockstate(_socket);

    if (status == CONNECTING)
        return conn_status::connecting;

    if (status == CONNECTED)
        return conn_status::connected;

    pfs::throw_or(perr, error {tr::f_("unexpected UDT socket state while connecting: {}"
        , static_cast<int>(status))});

    return conn_status::failure;
}

void udt_socket::disconnect (error * perr)
{
    (void)perr;
    UDT::close(_socket);
    _socket = kINVALID_SOCKET;
}

}} // namespace netty::udt
