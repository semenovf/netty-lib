////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/error.hpp"
#include "netty/namespace.hpp"
#include "netty/posix/udp_socket.hpp"
#include <pfs/endian.hpp>
#include <pfs/i18n.hpp>

#if _MSC_VER
#   include <winsock2.h>
#   include <ws2tcpip.h>
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#endif

NETTY__NAMESPACE_BEGIN

namespace posix {

udp_socket::udp_socket () : inet_socket() {}

udp_socket::udp_socket (udp_socket && s)
    : inet_socket(std::move(s))
{}

udp_socket & udp_socket::operator = (udp_socket && s)
{
    if (this != & s) {
        this->~udp_socket();
        inet_socket::operator = (std::move(s));
    }

    return *this;
}

udp_socket::~udp_socket () = default;

// https://learn.microsoft.com/en-us/windows/win32/winsock/multicast-programming
bool udp_socket::join (socket4_addr const & group_saddr
    , inet4_addr const & local_addr, error * perr)
{
    if (!*this) {
        if (!init(type_enum::dgram, perr))
            return false;
    }

    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(group_saddr.addr));
    mreq.imr_interface.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(local_addr));

#if _MSC_VER
    auto rc = ::setsockopt(_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<char const *>(& mreq), sizeof(mreq));
#else
    auto rc = ::setsockopt(_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, & mreq, sizeof(mreq));
#endif

    if (rc != 0) {
        pfs::throw_or(perr, error {
              errc::socket_error
            , tr::f_("join multicast group: {}", to_string(group_saddr))
            , pfs::system_error_text()
        });

        return false;
    }

    return true;
}

bool udp_socket::leave (socket4_addr const & group_saddr, inet4_addr const & local_addr, error * perr)
{
    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(group_saddr.addr));
    mreq.imr_interface.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(local_addr));

#if _MSC_VER
    auto rc = ::setsockopt(_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, reinterpret_cast<char const *>(& mreq), sizeof(mreq));
#else
    auto rc = ::setsockopt(_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, & mreq, sizeof(mreq));
#endif

    if (rc != 0) {
        pfs::throw_or(perr, error {
              errc::socket_error
            , tr::f_("leave multicast group: {}", to_string(group_saddr))
            , pfs::system_error_text()
        });

        return false;
    }

    return true;

}

bool udp_socket::enable_broadcast (bool enable, error * perr)
{
    if (!*this) {
        if (!init(type_enum::dgram, perr))
            return false;
    }

    const int on = enable ? 1 : 0;

#if _MSC_VER
    auto rc = ::setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char const *>(& on), sizeof(on));
#else
    auto rc = ::setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, & on, sizeof(on));
#endif

    if (rc != 0) {
        error err {
              errc::socket_error
            , tr::_("{} broadcast"
                , (enable ? tr::_("enable") : tr::_("disable")))
            , pfs::system_error_text()
        };

        if (perr) {
            *perr = std::move(err);
            return false;
        } else {
            throw err;
        }
    }

    return true;
}

} // namespace posix

NETTY__NAMESPACE_END
