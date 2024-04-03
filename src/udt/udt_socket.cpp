////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.26 Initial version.
//      2023.01.06 Renamed to `udt_socket` and refactored.
////////////////////////////////////////////////////////////////////////////////
#include "newlib/udt.hpp"
#include "pfs/endian.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/udt/udt_socket.hpp"
#include <cerrno>
#include <cassert>

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

int basic_socket::default_exp_counter ()
{
    return 2;
}

std::chrono::milliseconds basic_socket::default_exp_threshold ()
{
    return std::chrono::milliseconds{625};
}

basic_socket::basic_socket () = default;

basic_socket::basic_socket (type_enum, int mtu, int exp_max_counter
    , std::chrono::milliseconds exp_threshold)
{
    int ai_family   = AF_INET;    // AF_INET | AF_INET6
    int ai_socktype = SOCK_DGRAM; // SOCK_DGRAM | SOCK_STREAM
    int ai_protocol = 0;

    _socket = UDT::socket(ai_family, ai_socktype, ai_protocol);

    if (_socket == UDT::INVALID_SOCK) {
        throw error {
              errc::socket_error
            , tr::_("create UDT socket failure")
            , UDT::getlasterror_desc()
        };
    }

    bool true_value  = true;
    bool false_value = false;

    // UDT Options
    auto rc = UDT::setsockopt(_socket, 0, UDT_REUSEADDR, & true_value, sizeof(bool));

    if (rc != UDT::ERROR)
        rc = UDT::setsockopt(_socket, 0, UDT_SNDSYN, & false_value, sizeof(bool)); // sending is non-blocking

    if (rc != UDT::ERROR)
        rc = UDT::setsockopt(_socket, 0, UDT_RCVSYN, & false_value, sizeof(bool)); // receiving is non-blocking

    if (rc != UDT::ERROR) {
        int mtu_value = mtu;
        rc = UDT::setsockopt(_socket, 0, UDT_MSS, & mtu_value, sizeof(mtu_value));
    }

//     if (rc != UDT::ERROR) {
//         int bufsz = 10000000;
//         rc = UDT::setsockopt(_socket, 0, UDT_SNDBUF, & bufsz, sizeof(bufsz));
//     }

//     if (rc != UDT::ERROR) {
//         int bufsz = 10000000;
//         rc = UDT::setsockopt(_socket, 0, UDP_RCVBUF, & bufsz, sizeof(bufsz));
//     }

    if (rc != UDT::ERROR) {
        if (exp_max_counter < 0)
            exp_max_counter = default_exp_counter();

        if (exp_threshold < std::chrono::milliseconds{0})
            exp_threshold = default_exp_threshold();

        std::uint64_t exp_threshold_usecs = static_cast<std::uint64_t>(exp_threshold.count()) * 1000;

        rc = UDT::setsockopt(_socket, 0, UDT_EXP_MAX_COUNTER, & exp_max_counter, sizeof(exp_max_counter));

        if (rc != UDT::ERROR)
            rc = UDT::setsockopt(_socket, 0, UDT_EXP_THRESHOLD, & exp_threshold_usecs, sizeof(exp_threshold_usecs));

    }

    //UDT::setsockopt(_socket, 0, UDT_CC, new CCCFactory<debug_CCC>, sizeof(CCCFactory<debug_CCC>));

    if (rc == UDT::ERROR) {
        throw error {
              errc::socket_error
            , tr::_("UDT set socket option failure")
            , UDT::getlasterror_desc()
        };
    }
}

basic_socket::basic_socket (native_type sock, socket4_addr const & saddr)
    : _socket(sock)
    , _saddr(saddr)
{}

basic_socket::~basic_socket ()
{
    if (_socket >= 0) {
        UDT::close(_socket);
        _socket = kINVALID_SOCKET;
    }
}

basic_socket::native_type basic_socket::native () const noexcept
{
    return _socket;
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

std::vector<std::pair<std::string, std::string>> basic_socket::dump_options () const
{
    std::vector<std::pair<std::string, std::string>> result;
    int opt_size {0};

    // UDT_MSS - Maximum packet size (bytes). Including all UDT, UDP, and IP
    // headers. Default 1500 bytes.
    int mss {0};
    assert(0 == UDT::getsockopt(_socket, 0, UDT_MSS, & mss, & opt_size));
    result.push_back(std::make_pair("UDT_MSS", std::to_string(mss)
        + ' ' + "bytes (max packet size)"));

    // UDT_SNDSYN - Synchronization mode of data sending. True for blocking
    // sending; false for non-blocking sending. Default true.
    bool sndsyn {false};
    assert(0 == UDT::getsockopt(_socket, 0, UDT_SNDSYN, & sndsyn, & opt_size));
    result.push_back(std::make_pair("UDT_SNDSYN", sndsyn
        ? "TRUE (sending blocking)"
        : "FALSE (sending non-blocking)"));

    // UDT_RCVSYN - Synchronization mode for receiving.	true for blocking
    // receiving; false for non-blocking receiving. Default true.
    bool rcvsyn {false};
    assert(0 == UDT::getsockopt(_socket, 0, UDT_RCVSYN, & rcvsyn, & opt_size));
    result.push_back(std::make_pair("UDT_RCVSYN", rcvsyn
        ? "TRUE (receiving blocking)"
        : "FALSE (receiving non-blocking)"));

    // UDT_FC - Maximum window size (packets). Default 25600.
    int fc {0};
    assert(0 == UDT::getsockopt(_socket, 0, UDT_FC, & fc, & opt_size));
    result.push_back(std::make_pair("UDT_FC", std::to_string(fc)
        + ' ' + "packets (max window size)"));

    // UDT_STATE - Current status of the UDT socket.
    std::int32_t state {0};
    assert(0 == UDT::getsockopt(_socket, 0, UDT_STATE, & state, & opt_size));
    result.push_back(std::make_pair("UDT_STATE", state_string(state)));

    // UDT_LINGER - Linger time on close().
    linger lng {0, 0};
    int linger_sz = sizeof(linger);
    assert(0 == UDT::getsockopt(_socket, 0, UDT_LINGER, & lng, & linger_sz));
    result.push_back(std::make_pair("UDT_LINGER"
        , "{" + std::to_string(lng.l_onoff)
        + ", " + std::to_string(lng.l_linger) + "}"));

    // UDT_EVENT - The EPOLL events available to this socket.
    std::int32_t event {0};
    assert(0 == UDT::getsockopt(_socket, 0, UDT_EVENT, & event, & opt_size));

    std::string event_str;

    if (event & UDT_EPOLL_IN)
        event_str += (event_str.empty() ? std::string{} : " | ") + "UDT_EPOLL_IN";
    if (event & UDT_EPOLL_OUT)
        event_str += (event_str.empty() ? std::string{} : " | ") + "UDT_EPOLL_OUT";
    if (event & UDT_EPOLL_ERR)
        event_str += (event_str.empty() ? std::string{} : " | ") + "UDT_EPOLL_ERR";

    result.push_back(std::make_pair("UDT_EVENT", event_str.empty() ? "<empty>" : event_str));

    return result;
}

basic_udt_socket::basic_udt_socket (int mtu, int exp_max_counter
    , std::chrono::milliseconds exp_threshold)
    : basic_socket(type_enum::dgram, mtu, exp_max_counter, exp_threshold)
{}

basic_udt_socket::basic_udt_socket (uninitialized): basic_socket() {}

basic_udt_socket::basic_udt_socket (native_type sock, socket4_addr const & saddr)
    : basic_socket(sock, saddr)
{}

basic_udt_socket::basic_udt_socket (basic_udt_socket && other)
{
    this->operator = (std::move(other));
}

basic_udt_socket & basic_udt_socket::operator = (basic_udt_socket && other)
{
    this->~basic_udt_socket();
    _socket = other._socket;
    _saddr  = other._saddr;
    other._socket = kINVALID_SOCKET;
    return *this;
}

basic_udt_socket::~basic_udt_socket () = default;

basic_udt_socket::operator bool () const noexcept
{
    return _socket != kINVALID_SOCKET;
}

basic_udt_socket::native_type basic_udt_socket::native () const noexcept
{
    return _socket;
}

// TODO Need to correct handle error
int basic_udt_socket::recv (char * data, int len, error * /*perr*/)
{
    auto rc = UDT::recvmsg(_socket, data, len);

    if (rc == UDT::ERROR) {
        // Error code: 6002
        // Error desc: Non-blocking call failure: no data available for reading.
        if (CUDTException{6, 2, 0}.getErrorCode() == UDT::getlasterror_code())
            rc = 0;
    }

    return rc;
}

// TODO Need to correct handle error
send_result basic_udt_socket::send (char const * data, int len, error * /*perr*/)
{
    int ttl_millis = -1;
    bool inorder = true;

    // Return
    // > 0             - on success;
    // = 0             - if UDT_SNDTIMEO > 0 and the message cannot be sent
    //                   before the timer expires;
    // -1              - on error
    auto rc = UDT::sendmsg(_socket, data, len, ttl_millis, inorder);

    if (rc == UDT::ERROR) {
        LOGE(TAG, "SEND: code={}, text={}", UDT::getlasterror_code(), UDT::getlasterror_desc());

        // Error code: 6001
        // Error desc: Non-blocking call failure: no buffer available for sending.
        if (CUDTException{6, 1, 0}.getErrorCode() == UDT::getlasterror_code())
            return send_result{send_status::failure, 0};
    }

    return send_result{send_status::good, static_cast<decltype(send_result::n)>(rc)};
}

conn_status basic_udt_socket::connect (socket4_addr const & saddr, error * perr)
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
        error err {
              errc::socket_error
            , tr::_("socket connect error")
            , UDT::getlasterror_desc()
        };

        if (perr) {
            *perr = std::move(err);
        } else {
            throw err;
        }

        return conn_status::failure;
    }

    _saddr = saddr;

    auto status = UDT::getsockstate(_socket);

    if (status == CONNECTING)
        return conn_status::connecting;

    if (status == CONNECTED)
        return conn_status::connected;

    error err {
          errc::socket_error
        , tr::f_("unexpected UDT socket state while connecting: {}"
            , static_cast<int>(status))
    };

    if (perr) {
        *perr = std::move(err);
    } else {
        throw err;
    }

    return conn_status::failure;
}

void basic_udt_socket::disconnect (error * perr)
{
    (void)perr;
    UDT::close(_socket);
    _socket = kINVALID_SOCKET;
}

}} // namespace netty::udt
