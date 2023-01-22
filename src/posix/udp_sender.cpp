////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/endian.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/posix/udp_sender.hpp"

#if _MSC_VER
#   include <winsock2.h>
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#endif

namespace netty {
namespace posix {

udp_sender::udp_sender () : udp_socket() {}

udp_sender::udp_sender (udp_sender && s)
    : udp_socket(std::move(s))
{}

udp_sender & udp_sender::operator = (udp_sender && s)
{
    udp_sender::operator = (std::move(s));
    return *this;
}

udp_sender::~udp_sender () = default;

bool udp_sender::set_multicast_interface (inet4_addr const & local_addr
    , error * perr)
{
    auto success = bind(_socket, socket4_addr{local_addr, 0}, perr);

    if (!success)
        return false;

    in_addr local_interface;

    local_interface.s_addr = pfs::to_network_order(static_cast<std::uint32_t>(local_addr));

    auto rc = setsockopt(_socket, IPPROTO_IP, IP_MULTICAST_IF, & local_interface
        , sizeof(local_interface));

    if (rc != 0) {
        error err {
              make_error_code(errc::socket_error)
            , tr::f_("set multicast interface to: {}", to_string(local_addr))
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

bool udp_sender::enable_broadcast (bool enable, error * perr)
{
    return udp_socket::enable_broadcast(enable, perr);
}

}} // namespace netty::posix

