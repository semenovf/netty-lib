////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/error.hpp"
#include "pfs/netty/posix/inet_socket.hpp"
#include "pfs/i18n.hpp"

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
namespace posix {

inet_socket::inet_socket () = default;

inet_socket::inet_socket (native_type sock, socket4_addr const & saddr)
    : _socket(sock)
    , _saddr(saddr)
{}

inet_socket::inet_socket (type_enum socktype)
{
    int ai_family = AF_INET;
    int ai_socktype = -1;
    int ai_protocol = 0;

    switch (socktype) {
        case type_enum::stream:
            ai_socktype = SOCK_STREAM;
            break;
        case type_enum::dgram:
            ai_socktype = SOCK_DGRAM;
            break;
        default:
            break;
    }

    if (ai_socktype < 0) {
        throw error {
              make_error_code(errc::socket_error)
            , tr::_("bad/unsupported socket type")
        };
    }

    ai_socktype |= SOCK_NONBLOCK;

    _socket = ::socket(ai_family, ai_socktype, ai_protocol);

    if (_socket < 0) {
        throw error {
              make_error_code(errc::socket_error)
            , tr::_("create INET socket failure")
            , pfs::system_error_text()
        };
    }

    int yes = 1;
    int rc = 0;

#if defined(SO_REUSEADDR)
    if (rc == 0) rc = ::setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, & yes, sizeof(int));
#endif

#if defined(SO_KEEPALIVE)
    if (ai_socktype & SOCK_STREAM) {
        if (rc == 0) rc = ::setsockopt(_socket, SOL_SOCKET, SO_KEEPALIVE, & yes, sizeof(int));
    }
#endif

    if (rc != 0) {
        throw error {
              make_error_code(errc::socket_error)
            , tr::_("set socket option failure")
            , pfs::system_error_text()
        };
    }
}

inet_socket::inet_socket (inet_socket && other)
{
    this->operator = (std::move(other));
}

inet_socket & inet_socket::operator = (inet_socket && other)
{
    this->~inet_socket();
    _socket = other._socket;
    _saddr  = other._saddr;
    other._socket = INVALID_SOCKET;
    return *this;
}

inet_socket::~inet_socket ()
{
    if (_socket >= 0) {
        ::shutdown(_socket, SHUT_RDWR);
        ::close(_socket);
        _socket = INVALID_SOCKET;
    }
}

inet_socket::operator bool () const noexcept
{
    return _socket != INVALID_SOCKET;
}

inet_socket::native_type inet_socket::native () const noexcept
{
    return _socket;
}

std::streamsize inet_socket::recv (char * data, std::streamsize len)
{
    auto rc = ::recv(_socket, data, len, MSG_DONTWAIT);

    if (rc < 0) {
        if (errno == EAGAIN || (EAGAIN != EWOULDBLOCK && errno == EWOULDBLOCK))
            rc = 0;
    }

    return rc;
}

std::streamsize inet_socket::send (char const * data, std::streamsize len)
{
    std::streamsize total_sent = 0;

    while (len) {
        // MSG_NOSIGNAL flag means:
        // requests not to send SIGPIPE on errors on stream oriented sockets
        // when the other end breaks the connection.
        // The EPIPE error is still returned.
        //
        auto n = ::send(_socket, data + total_sent, len, MSG_NOSIGNAL);

        if (n < 0) {
            if (errno == EAGAIN || (EAGAIN != EWOULDBLOCK && errno == EWOULDBLOCK))
                continue;

            total_sent = -1;
            break;
        }

        total_sent += n;
        len -= n;
    }

    return total_sent;
}

}} // namespace netty::posix
