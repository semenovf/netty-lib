////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
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

/**
 * POSIX Inet UDP socket
 */
class udp_socket: public inet_socket
{
public:
    udp_socket (udp_socket const & s) = delete;
    udp_socket & operator = (udp_socket const & s) = delete;

    NETTY__EXPORT udp_socket ();
    NETTY__EXPORT udp_socket (udp_socket && s);
    NETTY__EXPORT udp_socket & operator = (udp_socket && s);
    NETTY__EXPORT ~udp_socket ();
};

}} // namespace netty::posix



