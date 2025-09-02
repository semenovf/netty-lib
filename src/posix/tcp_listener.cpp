////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/error.hpp"
#include "netty/namespace.hpp"
#include "netty/posix/tcp_listener.hpp"
#include <pfs/endian.hpp>
#include <pfs/i18n.hpp>

#if _MSC_VER
#   include <winsock2.h>
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#endif

NETTY__NAMESPACE_BEGIN

namespace posix {

tcp_listener::tcp_listener () : inet_socket() {}

tcp_listener::tcp_listener (socket4_addr const & saddr, netty::error * perr)
    : inet_socket()
{
    if (!init(inet_socket::type_enum::stream, perr))
        return;

    _saddr = saddr;
}

bool tcp_listener::listen (int backlog, error * perr)
{
    if (!bind(_socket, _saddr, perr))
        return false;

    auto rc = ::listen(_socket, backlog);

    if (rc != 0) {
        pfs::throw_or(perr, error {tr::f_("listen failure: {}", pfs::system_error_text())});
        return false;
    }

    return true;
}

tcp_socket tcp_listener::accept (error * perr)
{
    sockaddr_in sa;

#if _MSC_VER
    int addrlen = sizeof(sa);
    auto sock = ::accept(_socket, reinterpret_cast<sockaddr *>(& sa), & addrlen);
#else
    socklen_t addrlen = sizeof(sa);
    auto sock = ::accept(_socket, reinterpret_cast<sockaddr *>(& sa), & addrlen);
#endif

    if (sock > 0) {
        if (sa.sin_family == AF_INET) {
            auto addr = pfs::to_native_order(static_cast<std::uint32_t>(sa.sin_addr.s_addr));
            auto port = pfs::to_native_order(static_cast<std::uint16_t>(sa.sin_port));

            return tcp_socket{sock, socket4_addr{addr, port}};
        } else {
            pfs::throw_or(perr, error {
                tr::f_("socket accept failure: unsupported sockaddr family: {}"
                    " (AF_INET supported only)", sa.sin_family)
            });

            return tcp_socket{};
        }
    }

#if defined(EAGAIN) && defined(EWOULDBLOCK) && EAGAIN != EWOULDBLOCK
    if (errno == EAGAIN || errno == EWOULDBLOCK)
#else
    if (errno == EAGAIN)
#endif
        return tcp_socket{};

    pfs::throw_or(perr, error {tr::f_("socket accept failure: {}", pfs::system_error_text())});

    return tcp_socket{};
}

tcp_socket tcp_listener::accept_nonblocking (error * perr)
{
    auto s = accept(perr);

    if (!s.set_nonblocking(true, perr))
        return tcp_socket{};

    return s;
}

} // namespace posix

NETTY__NAMESPACE_END
