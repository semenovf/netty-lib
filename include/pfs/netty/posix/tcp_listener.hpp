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
#include "../exports.hpp"
#include "../error.hpp"
#include "../listener_options.hpp"
#include "tcp_socket.hpp"

NETTY__NAMESPACE_BEGIN

namespace posix {

/**
 * POSIX Inet TCP listener
 */
class tcp_listener: public inet_socket
{
public:
    using listener_id = inet_socket::socket_id;
    using socket_type = tcp_socket;

private:
    int _backlog = 10;

public:
    /**
     * Constructs invalid (uninitialized) TCP server.
     */
    NETTY__EXPORT tcp_listener ();

    /**
     * Constructs POSIX TCP server.
     */
    NETTY__EXPORT tcp_listener (socket4_addr const & saddr, int backlog = 10, error * perr = nullptr);

    /**
     * Constructs POSIX TCP listener using listener option set @a opts.
     */
    NETTY__EXPORT tcp_listener (listener_options const & opts, error * perr = nullptr);

public:
    /**
     * Bind the socket to address and listen for connections on a socket.
     *
     * @param backlog The maximum length to which the queue of pending connections may grow.
     */
    NETTY__EXPORT bool listen (error * perr = nullptr);

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

} // namespace posix

NETTY__NAMESPACE_END
