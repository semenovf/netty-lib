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
#include "tcp_socket.hpp"
#include <map>

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
     * Constructs POSIX TCP server using option set in @a opts.
     *
     * @details @a opts may/must contain the following keys:
     *          * addr - the address on which the listener will listen (any address by default);
     *          * port - the port on which the listener will listen (zero by default);
     *          * backlog - the maximum length to which the queue of pending connections may grow
     *              (10 by default); value must be in range [0, SOMAXCONN]; if the option value is
     *              zero, the actual value is set to the default value.
     */
    NETTY__EXPORT tcp_listener (std::map<std::string, std::string> const & opts, error * perr = nullptr);

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
