////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "udp_socket.hpp"
#include <functional>

namespace netty {
namespace posix {

/**
 * POSIX UDP receiver socket
 */
class udp_receiver: public udp_socket
{
    // Destructor handler, used by for multicast sender
    std::function<void()> _dtor;

public:
    /**
     * Constructs uninitialized (invalid) UDP receiver.
     */
    NETTY__EXPORT udp_receiver ();

    /**
     * Initializes multicast, broadcast or unicast receiver.
     *
     * @details If @a src_saddr is multicast address socket will be bind to
     *          address @c INADDR_ANY for @c Windows and to @a src_saddr in
     *          other cases. Then it will be joined to @a src_saddr on
     *          @a local_addr interface.
     *          If @a src_saddr is broadcast address socket will be bind to
     *          to @a src_saddr and @a local_addr will be ignored.
     *          If @a src_saddr is unicast address socket will be bind to
     *          to @a src_saddr and @a local_addr will be ignored.
     */
    NETTY__EXPORT udp_receiver (socket4_addr const & src_saddr
        , inet4_addr const & local_addr);

    /**
     * Initializes unicast or broadcast receiver with binding to @a local_saddr.
     */
    NETTY__EXPORT udp_receiver (socket4_addr const & local_saddr);

    udp_receiver (udp_receiver const & s) = delete;
    udp_receiver & operator = (udp_receiver const & s) = delete;

    NETTY__EXPORT udp_receiver (udp_receiver && s);
    NETTY__EXPORT udp_receiver & operator = (udp_receiver && s);
    NETTY__EXPORT ~udp_receiver ();
};

}} // namespace netty::posix


