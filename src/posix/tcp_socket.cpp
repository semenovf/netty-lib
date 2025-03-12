////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/error.hpp"
#include "netty/namespace.hpp"
#include "netty/posix/tcp_socket.hpp"
#include <pfs/endian.hpp>
#include <pfs/i18n.hpp>
#include <memory>

#if _MSC_VER
#   include <winsock2.h>
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#endif

NETTY__NAMESPACE_BEGIN

namespace posix {

tcp_socket::tcp_socket () : inet_socket() {}

// Accepted socket
tcp_socket::tcp_socket (socket_id sock, socket4_addr const & saddr)
    : inet_socket(sock, saddr)
{}

tcp_socket::tcp_socket (tcp_socket && other)
    : inet_socket(std::move(other))
{}

tcp_socket & tcp_socket::operator = (tcp_socket && other)
{
    inet_socket::operator = (std::move(other));
    return *this;
}

tcp_socket::~tcp_socket () = default;

conn_status tcp_socket::connect (socket4_addr const & remote_saddr, inet4_addr const & local_addr
    , error * perr)
{
    if (!init(type_enum::stream, perr))
        return conn_status::failure;

    if (local_addr != any_inet4_addr()) {
        auto success = bind(_socket, socket4_addr{local_addr, 0}, perr);

        if (!success)
            return conn_status::failure;
    }

    sockaddr_in addr_in4;

    memset(& addr_in4, 0, sizeof(addr_in4));

    addr_in4.sin_family      = AF_INET;
    addr_in4.sin_port        = pfs::to_network_order(static_cast<std::uint16_t>(remote_saddr.port));
    addr_in4.sin_addr.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(remote_saddr.addr));
    _saddr = remote_saddr;

    auto rc = ::connect(_socket, reinterpret_cast<sockaddr *>(& addr_in4), sizeof(addr_in4));

    bool in_progress = false;

    if (rc < 0) {
#if _MSC_VER
        auto lastWsaError = WSAGetLastError();
        in_progress = (lastWsaError == WSAEINPROGRESS || lastWsaError == WSAEWOULDBLOCK);
#else
        in_progress = (errno == EINPROGRESS || errno == EWOULDBLOCK);
#endif

        if (!in_progress) {
#if _MSC_VER
            auto unreachable = (lastWsaError == WSAENETUNREACH || errno == WSAENETDOWN);
#else
            auto unreachable = (errno == ENETUNREACH || errno == ENETDOWN);
#endif
            if (unreachable) {
                return conn_status::unreachable;
            } else {
                pfs::throw_or(perr, error {tr::f_("socket connect error: {}", pfs::system_error_text())});
                return conn_status::failure;
            }
        }
    }

    return in_progress ? conn_status::connecting : conn_status::connected;
}

conn_status tcp_socket::connect (socket4_addr const & remote_saddr, error * perr)
{
    return connect(remote_saddr, any_inet4_addr(), perr);
}

void tcp_socket::disconnect (error * perr)
{
    if (_socket > 0) {
        auto rc = ::shutdown(_socket
#if _MSC_VER
            , SD_BOTH);
#else
            , SHUT_RDWR);
#endif

        if (rc != 0) {
#if _MSC_VER
            auto lastWsaError = WSAGetLastError();

            if (lastWsaError != WSAENOTCONN && lastWsaError != WSAECONNRESET) {
#else // _MSC_VER
            if (errno != ENOTCONN && errno != ECONNRESET) {
#endif // POSIX
                pfs::throw_or(perr, error {tr::f_("socket shutdown error: {}", pfs::system_error_text())});
            }
        }
    }
}

} // namespace posix

NETTY__NAMESPACE_END
