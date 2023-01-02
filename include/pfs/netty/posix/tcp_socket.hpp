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

class tcp_server;

/**
 * POSIX Inet TCP socket
 */
class tcp_socket: public inet_socket
{
    friend class tcp_server;

private:
    enum class state_enum {
          unconnected // The socket is not connected (initial state).
        , connecting  // The socket has started establishing a connection.
        , connected   // A connection is established.
    };

private:
    state_enum _state {state_enum::unconnected};

protected:
    /**
     * Constructs POSIX TCP accepted socket.
     */
    tcp_socket (native_type sock, socket4_addr const & saddr);

    /**
     * Constructs unitialized (invalid) TCP socket.
     */
    tcp_socket (bool);

public:
    tcp_socket (tcp_socket const & s) = delete;
    tcp_socket & operator = (tcp_socket const & s) = delete;

    NETTY__EXPORT tcp_socket ();

    NETTY__EXPORT tcp_socket (tcp_socket && s);
    NETTY__EXPORT tcp_socket & operator = (tcp_socket && s);
    NETTY__EXPORT ~tcp_socket ();

    /**
     * Connects to the TCP server.
     *
     * @return @c true if socket immediatly connected, or @c false if connection
     *         in progress or if an error occurred and @a perr is not @c nullptr.
     */
    NETTY__EXPORT bool connect (socket4_addr const & saddr, error * perr = nullptr);

    /**
     * Checks if the TCP socket in a connected state.
     */
    NETTY__EXPORT bool connected () const;

    /**
     * Checks if the TCP socket connected.
     *
     * @details If socket has last state `connecting`, then a check is perfomed
     *          if it already connected. Must be called in poller's `on_error`
     *          handler to correct triggering state and filtering unnecessary
     *          events.
     *
     * @return @c true if socket in a connected state, @c false otherwise.
     */
    NETTY__EXPORT bool ensure_connected ();
};

}} // namespace netty::posix
