////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.12.31 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/endian.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/p2p/posix/tcp_socket.hpp"
#include <cerrno>
#include <cassert>
#include <memory>

#if _MSC_VER
#   include <winsock2.h>
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <string.h>
#   include <unistd.h>
#endif

namespace netty {
namespace p2p {
namespace posix {

static tcp_socket::native_type create (inet4_addr const & addr
    , std::uint16_t port
    , sockaddr_in & addr_in4)
{
    int ai_family   = AF_INET;    // AF_INET | AF_INET6
    int ai_socktype = SOCK_STREAM;
    int ai_protocol = 0;

    memset(& addr_in4, 0, sizeof(addr_in4));

    addr_in4.sin_family      = ai_family;
    addr_in4.sin_port        = pfs::to_network_order(static_cast<std::uint16_t>(port));
    addr_in4.sin_addr.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(addr));

    tcp_socket::native_type sock = ::socket(ai_family, ai_socktype, ai_protocol);

    if (sock == tcp_socket::INVALID_SOCKET) {
        throw error {
              pfs::get_last_system_error()
            , tr::f_("create socket failure: {}:{}", to_string(addr), port)
        };
    }

    int yes = 1;
    int rc = 0;

#if defined(SO_REUSEADDR)
    if (rc == 0) rc = ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, & yes, sizeof(int));
#endif

#if defined(SO_KEEPALIVE)
    if (rc == 0) rc = ::setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, & yes, sizeof(int));
#endif

    if (rc != 0) {
        throw error {
              pfs::get_last_system_error()
            , tr::f_("set socket option failure: {}:{}", to_string(addr), port)
        };
    }

    return sock;
}

tcp_socket::~tcp_socket ()
{
    close();
}

// tcp_socket::state_enum tcp_socket::state () const
// {
//     assert(_socket >= 0);
//     auto status = UDT::getsockstate(_socket);
//     return static_cast<state_enum>(status);
// }

void tcp_socket::bind (inet4_addr addr, std::uint16_t port)
{
    sockaddr_in addr_in4;
    _socket = create(addr, port, addr_in4);

    auto rc = ::bind(_socket
        , reinterpret_cast<sockaddr *>(& addr_in4)
        , sizeof(addr_in4));

    if (rc != 0) {
        throw error {
              pfs::get_last_system_error()
            , tr::f_("bind socket failure: {}:{}", to_string(addr), port)
        };
    }

    _saddr.addr = addr;
    _saddr.port = port;
}

void tcp_socket::listen (int backlog)
{
    PFS__ASSERT(_socket >= 0, "");

    auto rc = ::listen(_socket, backlog);

    if (rc < 0) {
        throw error {
              pfs::get_last_system_error()
            , tr::f_("listen failure")
        };
    }
}

tcp_socket tcp_socket::accept ()
{
    tcp_socket result;

    sockaddr sa;
    socklen_t addrlen;

    result._socket = ::accept(_socket, & sa, & addrlen);

    if (sa.sa_family == AF_INET) {
        auto addr_in4_ptr = reinterpret_cast<sockaddr_in *>(& sa);

        result._saddr.addr = pfs::to_native_order(
            static_cast<std::uint32_t>(addr_in4_ptr->sin_addr.s_addr));

        result._saddr.port = pfs::to_native_order(
            static_cast<std::uint16_t>(addr_in4_ptr->sin_port));
    } else {
        throw error {
              make_error_code(errc::socket_error)
            , tr::_("socket accept failure: unsupported sockaddr family"
                " (AF_INET supported only)")
        };
    }

    return result;
}

void tcp_socket::connect (inet4_addr addr, std::uint16_t port)
{
    sockaddr_in addr_in4;

    _socket = create(addr, port, addr_in4);

    auto rc = ::connect(_socket
        , reinterpret_cast<sockaddr *>(& addr_in4)
        , sizeof(addr_in4));

    if (rc < 0) {
        throw error{
              pfs::get_last_system_error()
            , tr::f_("connect failure to: {}:{}", to_string(addr), port)
        };
    }

    _saddr.addr = addr;
    _saddr.port = port;
}

void tcp_socket::close ()
{
    if (_socket >= 0) {
        ::close(_socket);
        _socket = INVALID_SOCKET;
    }
}

std::vector<std::pair<std::string, std::string>> tcp_socket::dump_options () const
{
    std::vector<std::pair<std::string, std::string>> result;
    socklen_t opt_size {0};
    int yes;

#if defined(SO_REUSEADDR)
    if (::getsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, & yes, & opt_size) == 0)
        result.push_back(std::make_pair("REUSEADDR", yes? "TRUE" : "FALSE"));

#endif

#if defined(SO_KEEPALIVE)
    if (::getsockopt(_socket, SOL_SOCKET, SO_KEEPALIVE, & yes, & opt_size) == 0)
        result.push_back(std::make_pair("KEEPALIVE", yes? "TRUE" : "FALSE"));
#endif

    return result;
}

std::streamsize tcp_socket::recvmsg (char * data, std::streamsize len)
{
    auto rc = ::recv(_socket, data, len, MSG_DONTWAIT);

    if (rc < 0) {
        if (errno == EAGAIN)
            rc = 0;
    }

    return rc;
}

std::streamsize tcp_socket::sendmsg (char const * data, std::streamsize len)
{
    std::streamsize total_sent = 0;

    while (len) {
        // MSG_NOSIGNAL flag means:
        // requests not to send SIGPIPE on errors on stream oriented sockets
        // when the other end breaks the connection.
        // The EPIPE error is still returned.
        //
        ssize_t n = ::send(_socket, data + total_sent, len, MSG_NOSIGNAL);

        if (n < 0) {
            if (errno == EAGAIN
                    || (EAGAIN != EWOULDBLOCK && errno == EWOULDBLOCK))
                continue;

            total_sent = -1;
            break;
        }

        total_sent += n;
        len -= n;
    }

    return total_sent;
}

// std::string tcp_socket::error_string () const noexcept
// {
//     return UDT::getlasterror_desc();
// }
//
// int udp_socket::error_code () const noexcept
// {
//     return UDT::getlasterror_code();
// }
//
// bool udp_socket::overflow () const noexcept
// {
//     return error_code() == 6001;
// }

// std::string udp_socket::state_string (int state)
// {
//     switch (state) {
//         case INIT      : return "INIT";
//         case OPENED    : return "OPENED";
//         case LISTENING : return "LISTENING";
//         case CONNECTING: return "CONNECTING";
//         case CONNECTED : return "CONNECTED";
//         case BROKEN    : return "BROKEN";
//         case CLOSING   : return "CLOSING";
//         case CLOSED    : return "CLOSED";
//         case NONEXIST  : return "NONEXIST";
//     }
//
//     return std::string{"<INVALID STATE>"};
// }

}}} // namespace netty::p2p::posix
