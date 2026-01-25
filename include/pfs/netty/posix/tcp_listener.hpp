////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
//      2024.05.14 Renamed to tcp_listener.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/exports.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"

namespace netty {
namespace posix {

/**
 * POSIX Inet TCP listener
 */
class tcp_listener: public inet_socket
{
public:
    using listener_id = inet_socket::socket_id;
    using socket_type = tcp_socket;

public:
    /**
     * Constructs invalid (uninitialized) TCP server.
     */
    NETTY__EXPORT tcp_listener ();

    /**
     * Constructs POSIX TCP server.
     */
    NETTY__EXPORT tcp_listener (socket4_addr const & saddr, error * perr = nullptr);

public:
    /**
     * Bind the socket to address and listen for connections on a socket.
     *
     * @param backlog The maximum length to which the queue of pending connections may grow.
     */
    NETTY__EXPORT bool listen (int backlog, error * perr = nullptr);

    /**
     * Accept a connection on a server socket.
     *
     * @return Accepted socket.
     */
    NETTY__EXPORT socket_type accept (error * perr = nullptr);

    /**
     * Accept a connection on a server socket.
     *
     * @return Accepted socket in non-blocking mode.
     */
    NETTY__EXPORT socket_type accept_nonblocking (error * perr = nullptr);

    // For compatiblity with listener_pool
    socket_type accept_nonblocking (listener_id /*id*/, error * perr = nullptr)
    {
        return accept_nonblocking(perr);
    }
};

}} // namespace netty::posix

