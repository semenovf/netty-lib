////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/posix/tcp_socket.hpp"
#include <chrono>

namespace netty {
namespace posix {

/**
 * POSIX Inet TCP server
 */
class tcp_server: public inet_socket
{
public:
    /**
     * Constructs POSIX TCP server and bind to the specified address.
     */
    tcp_server (socket4_addr const & saddr);

    /**
     * Constructs POSIX TCP server, bind to the specified address and start
     * listening
     */
    tcp_server (socket4_addr const & addr, int backlog);

    /**
     * Listen for connections on a socket.
     *
     * @param backlog The maximum length to which the queue of pending
     *        connections may grow.
     */
    bool listen (int backlog, error * perr = nullptr);

    /**
     * Aaccept a connection on a server socket.
     */
    tcp_socket accept (error * perr = nullptr);
};

}} // namespace netty::posix
