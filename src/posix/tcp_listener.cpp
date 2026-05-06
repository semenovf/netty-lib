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
#include <pfs/assert.hpp>
#include <pfs/endian.hpp>
#include <pfs/i18n.hpp>
#include <pfs/integer.hpp>

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

tcp_listener::tcp_listener (socket4_addr const & saddr, int backlog, netty::error * perr)
    : inet_socket()
    , _backlog(backlog)
{
    if (!init(inet_socket::type_enum::stream, perr))
        return;

    _saddr = saddr;
}

tcp_listener::tcp_listener (std::map<std::string, std::string> const & opts, error * perr)
{
    inet4_addr addr {inet4_addr::any_addr_value};
    std::uint16_t port = 0;
    int backlog = 10;

    for (auto const & o: opts) {
        if (o.first == "addr") {
            auto oaddr = inet4_addr::parse(o.second);

            if (!oaddr) {
                error err;
                auto addrs = inet4_addr::resolve(o.second, & err);

                if (err) {
                    pfs::throw_or(perr, std::make_error_code(std::errc::invalid_argument)
                        , tr::f_("bad value for listener address: {}", o.second));
                    return;
                }

                PFS__THROW_UNEXPECTED(addrs.empty(), "Expected non-empty address list");

                // Take first resolved address
                addr = addrs[0];
            } else {
                addr = *oaddr;
            }
        } else if (o.first == "port") {
            std::error_code ec;
            port = pfs::to_integer<std::uint16_t>(o.second, 0, std::numeric_limits<std::uint16_t>::max()
                , 10, ec);

            if (ec) {
                pfs::throw_or(perr, std::make_error_code(std::errc::invalid_argument)
                    , tr::f_("bad value for listener port: {}", o.second));
                return;
            }
        } else if (o.first == "backlog") {
            std::error_code ec;
            backlog = pfs::to_integer<int>(o.second, 0, SOMAXCONN, 10, ec);

            if (ec) {
                pfs::throw_or(perr, std::make_error_code(std::errc::invalid_argument)
                    , tr::f_("bad value for listener backlog: {}", o.second));
                return;
            }
        }
    }

    _saddr = socket4_addr{addr, port};
    _backlog = backlog;
}

bool tcp_listener::listen (error * perr)
{
    if (!bind(_socket, _saddr, perr))
        return false;

    auto rc = ::listen(_socket, _backlog);

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
