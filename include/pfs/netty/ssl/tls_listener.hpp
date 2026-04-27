////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "tls_socket.hpp"
#include "tls_options.hpp"
#include <memory>

NETTY__NAMESPACE_BEGIN

namespace ssl {

class tls_listener
{
    class impl;

public:
    using listener_id = tls_socket::socket_id;
    using socket_type = tls_socket;

private:
    std::unique_ptr<impl> _d;

public:
    /**
     * Constructs invalid (uninitialized) server.
     */
    NETTY__EXPORT tls_listener ();

    /**
     */
    NETTY__EXPORT tls_listener (tls_listener && other) noexcept;

    /**
     * Constructs server.
     */
    NETTY__EXPORT tls_listener (socket4_addr const & saddr, tls_options opts, error * perr = nullptr);

    /**
     */
    NETTY__EXPORT tls_listener & operator = (tls_listener && other) noexcept;

    NETTY__EXPORT ~tls_listener ();

public:
    NETTY__EXPORT operator bool () const noexcept;

    NETTY__EXPORT listener_id id () const noexcept;

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

private:
    socket_type accept (bool force_nonblocking, error * perr = nullptr);
};

} // namespace ssl

NETTY__NAMESPACE_END
