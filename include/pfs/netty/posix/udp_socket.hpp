////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/posix/inet_socket.hpp"

namespace netty {
namespace posix {

// enum class cast_enum { unicast, multicast, broadcast };

/**
 * POSIX Inet UDP socket
 */
class udp_socket: public inet_socket
{
protected:
    /**
     * Constructs uninitialized (invalid) UDP socket.
     */
    udp_socket (uninitialized);

    /**
      * Joins the multicast group specified by @a group on the default interface
      * chosen by the operating system.
      *
      * @param group_addr Multicast group address.
      * @param local_addr Local address to bind and to set the multicast interface.
      *
      * @return @c true if successful; otherwise it returns @c false.
      */
    bool join (socket4_addr const & group_saddr
        , inet4_addr const & local_addr, error * perr = nullptr);

    /**
     * Leaves the multicast group specified by @a group groupAddress on the
     * default interface chosen by the operating system.
     *
     * @return @c true if successful; otherwise it returns @c false.
     */
    bool leave (socket4_addr const & group_saddr
        , inet4_addr const & local_addr, error * perr = nullptr);

    bool enable_broadcast (bool enable, error * perr = nullptr);

public:
    udp_socket (udp_socket const & s) = delete;
    udp_socket & operator = (udp_socket const & s) = delete;

    NETTY__EXPORT udp_socket ();
    NETTY__EXPORT udp_socket (udp_socket && s);
    NETTY__EXPORT udp_socket & operator = (udp_socket && s);
    NETTY__EXPORT ~udp_socket ();
};

}} // namespace netty::posix
