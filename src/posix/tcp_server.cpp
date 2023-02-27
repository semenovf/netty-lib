////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/error.hpp"
#include "pfs/netty/posix/tcp_server.hpp"
#include "pfs/endian.hpp"
#include "pfs/i18n.hpp"

#if _MSC_VER
#   include <winsock2.h>
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#endif

namespace netty {
namespace posix {

tcp_server::tcp_server () : inet_socket() {}

tcp_server::tcp_server (socket4_addr const & saddr)
    : inet_socket(inet_socket::type_enum::stream)
{
    if (bind(_socket, saddr, nullptr))
        _saddr = saddr;
}

tcp_server::tcp_server (socket4_addr const & saddr, int backlog)
    : tcp_server(saddr)
{
    listen(backlog);
}

bool tcp_server::listen (int backlog, error * perr)
{
    auto rc = ::listen(_socket, backlog);

    if (rc != 0) {
        error err {
              errc::socket_error
            , tr::_("listen failure")
            , pfs::system_error_text()
        };

        if (perr) {
            *perr = std::move(err);
        } else {
            throw err;
        }

        return false;
    }

    return true;
}

tcp_socket tcp_server::accept (native_type listener_sock, error * perr)
{
    sockaddr_in sa;

#if _MSC_VER
    int addrlen = sizeof(sa);
    auto sock = ::accept(listener_sock, reinterpret_cast<sockaddr *>(& sa), & addrlen);
#else
    socklen_t addrlen = sizeof(sa);
    auto sock = ::accept(listener_sock, reinterpret_cast<sockaddr *>(& sa), & addrlen);
#endif

    if (sock > 0) {
        if (sa.sin_family == AF_INET) {
            auto addr = pfs::to_native_order(static_cast<std::uint32_t>(sa.sin_addr.s_addr));
            auto port = pfs::to_native_order(static_cast<std::uint16_t>(sa.sin_port));

            return tcp_socket{sock, socket4_addr{addr, port}};
        } else {
            error err {
                  errc::socket_error
                , tr::f_("socket accept failure: unsupported sockaddr family: {}"
                        " (AF_INET supported only)", sa.sin_family)
            };

            if (perr) {
                *perr = std::move(err);
            } else {
                throw err;
            }
        }
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return tcp_socket{uninitialized{}};
    }

    error err {
          errc::socket_error
        , tr::_("socket accept failure")
        , pfs::system_error_text()
    };

    if (perr) {
        *perr = std::move(err);
    } else {
        throw err;
    }

    return tcp_socket{uninitialized{}};
}

tcp_socket tcp_server::accept_nonblocking (native_type listener_sock, error * perr)
{
    auto s = accept(listener_sock, perr);

    if (!s.set_nonblocking(true, perr))
        return tcp_socket{uninitialized{}};

    return s;
}

tcp_socket tcp_server::accept (error * perr)
{
    return accept(_socket, perr);
}

tcp_socket tcp_server::accept_nonblocking (error * perr)
{
    return accept_nonblocking(_socket, perr);
}

}} // namespace netty::posix
