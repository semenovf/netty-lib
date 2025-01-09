////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/error.hpp"
#include "netty/namespace.hpp"
#include "netty/posix/udp_receiver.hpp"
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

udp_receiver::udp_receiver () : udp_socket(uninitialized{}) {}

udp_receiver::udp_receiver (socket4_addr const & src_saddr
    , inet4_addr const & local_addr) : udp_socket()
{
    if (is_multicast(src_saddr.addr)) {
#if _MSC_VER
        bind(_socket, socket4_addr{INADDR_ANY, src_saddr.port}, nullptr);
#else
        bind(_socket, src_saddr, nullptr);
#endif

        join(src_saddr, local_addr);

        _dtor = [src_saddr, local_addr] (udp_receiver * that) {
            that->leave(src_saddr, local_addr);
        };
    } else if (is_broadcast(src_saddr.addr)) {
        bind(_socket, src_saddr, nullptr);
        enable_broadcast(true);
    } else {
        bind(_socket, src_saddr, nullptr);
    }
}

udp_receiver::udp_receiver (socket4_addr const & local_saddr)
{
    if (is_multicast(local_saddr.addr)) {
        throw error {
              errc::socket_error
            , tr::f_("expected unicast or broadcast address: {}"
                , to_string(local_saddr.addr))
        };
    }

    bind(_socket, local_saddr, nullptr);

    if (is_broadcast(local_saddr.addr))
        enable_broadcast(true);
}

udp_receiver::udp_receiver (udp_receiver && s)
    : udp_socket(std::move(s))
    , _dtor(std::move(s._dtor))
{
    s._dtor = nullptr;
}

udp_receiver & udp_receiver::operator = (udp_receiver && s)
{
    this->~udp_receiver();
    udp_socket::operator = (std::move(s));
    _dtor = std::move(s._dtor);
    s._dtor = nullptr;
    return *this;
}

udp_receiver::~udp_receiver ()
{
    if (_dtor)
        _dtor(this);
}

} // namespace posix

NETTY__NAMESPACE_END
