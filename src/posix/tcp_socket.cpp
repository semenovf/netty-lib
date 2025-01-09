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

tcp_socket::tcp_socket () : inet_socket(type_enum::stream) {}

tcp_socket::tcp_socket (uninitialized) : inet_socket() {}

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

conn_status tcp_socket::connect (socket4_addr const & saddr, error * perr)
{
    sockaddr_in addr_in4;

    memset(& addr_in4, 0, sizeof(addr_in4));

    addr_in4.sin_family      = AF_INET;
    addr_in4.sin_port        = pfs::to_network_order(static_cast<std::uint16_t>(saddr.port));
    addr_in4.sin_addr.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(saddr.addr));

    auto rc = ::connect(_socket, reinterpret_cast<sockaddr *>(& addr_in4), sizeof(addr_in4));

    bool in_progress = false;

    if (rc < 0) {
#if _MSC_VER
        auto lastWsaError = WSAGetLastError();
        in_progress = lastWsaError == WSAEINPROGRESS || lastWsaError == WSAEWOULDBLOCK;
#else
        in_progress = (errno == EINPROGRESS || errno == EWOULDBLOCK);
#endif

        if (!in_progress) {
            error err {
                  errc::socket_error
                , tr::_("socket connect error")
                , pfs::system_error_text()
            };

            if (perr) {
                *perr = std::move(err);
            } else {
                throw err;
            }

            return conn_status::failure;
        }
    }

    _saddr = saddr;

    return in_progress ? conn_status::connecting : conn_status::connected;
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
                error err {
                      errc::socket_error
                    , tr::_("socket shutdown error")
                    , pfs::system_error_text()
                };

                if (perr) {
                    *perr = std::move(err);
                } else {
                    throw err;
                }
            }
        }
    }
}

} // namespace posix

NETTY__NAMESPACE_END
