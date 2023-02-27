////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/exports.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"

namespace netty {
namespace posix {

/**
 * POSIX Inet TCP server
 */
class tcp_server: public inet_socket
{
public:
    /**
     * Constructs invalid (uninitialized) TCP server.
     */
    NETTY__EXPORT tcp_server ();

    /**
     * Constructs POSIX TCP server and bind to the specified address.
     */
    NETTY__EXPORT tcp_server (socket4_addr const & saddr);

    /**
     * Constructs POSIX TCP server, bind to the specified address and start
     * listening
     */
    NETTY__EXPORT tcp_server (socket4_addr const & addr, int backlog);

    /**
     * Listen for connections on a socket.
     *
     * @param backlog The maximum length to which the queue of pending
     *        connections may grow.
     */
    NETTY__EXPORT bool listen (int backlog, error * perr = nullptr);

    /**
     * Accept a connection on a server socket.
     */
    NETTY__EXPORT tcp_socket accept (error * perr = nullptr);
    NETTY__EXPORT tcp_socket accept_nonblocking (error * perr = nullptr);

public:
    NETTY__EXPORT static tcp_socket accept (native_type listener_sock
        , error * perr = nullptr);
    NETTY__EXPORT static tcp_socket accept_nonblocking (native_type listener_sock
        , error * perr = nullptr);
};

}} // namespace netty::posix
