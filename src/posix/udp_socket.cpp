////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/endian.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/posix/udp_socket.hpp"

#if _MSC_VER
#   include <winsock2.h>
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#endif

namespace netty {
namespace posix {

udp_socket::udp_socket () : inet_socket(type_enum::dgram) {}

udp_socket::udp_socket (uninitialized) : inet_socket() {}

udp_socket::udp_socket (udp_socket && s)
    : inet_socket(std::move(s))
{}

udp_socket & udp_socket::operator = (udp_socket && s)
{
    inet_socket::operator = (std::move(s));
    return *this;
}

udp_socket::~udp_socket () = default;

// https://learn.microsoft.com/en-us/windows/win32/winsock/multicast-programming
bool udp_socket::join (socket4_addr const & group_saddr
    , inet4_addr const & local_addr, error * perr)
{
    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(group_saddr.addr));
    mreq.imr_interface.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(local_addr));

    auto rc = setsockopt(_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, & mreq, sizeof(mreq));

    if (rc != 0) {
        error err {
              make_error_code(errc::socket_error)
            , tr::f_("join multicast group: {}", to_string(group_saddr))
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

bool udp_socket::leave (socket4_addr const & group_saddr
    , inet4_addr const & local_addr, error * perr)
{
    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(group_saddr.addr));
    mreq.imr_interface.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(local_addr));

    auto rc = setsockopt(_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, & mreq, sizeof(mreq));

    if (rc != 0) {
        error err {
              make_error_code(errc::socket_error)
            , tr::f_("leave multicast group: {}", to_string(group_saddr))
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

bool udp_socket::enable_broadcast (bool enable, error * perr)
{
    const int on = enable ? 1 : 0;
    auto rc = setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, & on, sizeof(on));

    if (rc != 0) {
        error err {
              make_error_code(errc::socket_error)
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

}} // namespace netty::posix
