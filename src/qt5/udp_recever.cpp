////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.18 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/endian.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/qt5/udp_receiver.hpp"

namespace netty {
namespace qt5 {

udp_receiver::udp_receiver () : udp_socket(uninitialized{}) {}

udp_receiver::udp_receiver (socket4_addr const & src_saddr
    , inet4_addr const & local_addr) : udp_socket()
{
    if (is_multicast(src_saddr.addr)) {
// #if _MSC_VER
//         bind(_socket, socket4_addr{INADDR_ANY, src_saddr.port});
// #else
//         bind(_socket, src_saddr);
// #endif
//
//         join(src_saddr, local_addr);
//
//         _dtor = [this, src_saddr, local_addr] () {
//             leave(src_saddr, local_addr);
//         };
    } else if (is_broadcast(src_saddr.addr)) {
//         bind(_socket, src_saddr);
//         enable_broadcast(true);
    } else {
//         bind(_socket, src_saddr);
    }
}

udp_receiver::udp_receiver (socket4_addr const & local_saddr)
{
//     if (is_multicast(local_saddr.addr)) {
//         throw error {
//               make_error_code(errc::socket_error)
//             , tr::f_("expected unicast or broadcast address: {}"
//                 , to_string(local_saddr.addr))
//         };
//     }
//
//     bind(_socket, local_saddr);
//
//     if (is_broadcast(local_saddr.addr))
//         enable_broadcast(true);
}

udp_receiver::udp_receiver (udp_receiver && s) = default;
udp_receiver & udp_receiver::operator = (udp_receiver && s) = default;
udp_receiver::~udp_receiver () = default;

}} // namespace netty::qt5
