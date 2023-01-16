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

tcp_server::tcp_server (socket4_addr const & saddr)
    : inet_socket(inet_socket::type_enum::stream)
{
    if (bind(_socket, saddr))
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
              make_error_code(errc::socket_error)
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
    sockaddr sa;
    socklen_t addrlen;

    auto sock = ::accept(listener_sock, & sa, & addrlen);

    if (sock > 0) {
        if (sa.sa_family == AF_INET) {
            auto addr_in4_ptr = reinterpret_cast<sockaddr_in *>(& sa);

            auto addr = pfs::to_native_order(
                static_cast<std::uint32_t>(addr_in4_ptr->sin_addr.s_addr));

            auto port = pfs::to_native_order(
                static_cast<std::uint16_t>(addr_in4_ptr->sin_port));

            return tcp_socket{sock, socket4_addr{addr, port}};
        } else {
            error err {
                make_error_code(errc::socket_error)
                , tr::_("socket accept failure: unsupported sockaddr family"
                        " (AF_INET supported only)")
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
          make_error_code(errc::socket_error)
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

tcp_socket tcp_server::accept (error * perr)
{
    return accept(_socket, perr);
}

}} // namespace netty::posix
