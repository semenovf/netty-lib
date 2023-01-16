////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/posix/udp_socket.hpp"
#include <functional>

namespace netty {
namespace posix {

/**
 * POSIX UDP sender socket
 */
class udp_sender: public udp_socket
{
public:
    udp_sender (udp_sender const & s) = delete;
    udp_sender & operator = (udp_sender const & s) = delete;

    /**
     * Constructs UDP sender.
     */
    NETTY__EXPORT udp_sender ();

    NETTY__EXPORT udp_sender (udp_sender && s);
    NETTY__EXPORT udp_sender & operator = (udp_sender && s);
    NETTY__EXPORT ~udp_sender ();

    /**
     * Sets the outgoing interface specified by @a addr for multicast datagrams.
     */
    NETTY__EXPORT bool set_multicast_interface (inet4_addr const & local_addr
        , error * perr = nullptr);

    /**
     * Enables/disables broadcast send.
     *
     * @param enable Enable (@c true) or disable (@c false) broadcast send.
     *
     * @return @c true if successful; otherwise it returns @c false.
     */
    NETTY__EXPORT bool enable_broadcast (bool enable, error * perr = nullptr);
};

}} // namespace netty::posix

