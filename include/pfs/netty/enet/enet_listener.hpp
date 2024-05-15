////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/error.hpp>
#include <pfs/netty/exports.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <pfs/netty/enet/enet_socket.hpp>

namespace netty {
namespace enet {

class enet_listener
{
public:
    using native_type = enet_socket::native_type;

public:
    /**
     * Constructs invalid (uninitialized) ENet listener.
     */
    NETTY__EXPORT enet_listener ();

    /**
     * Constructs ENet server and bind to the specified address.
     */
    NETTY__EXPORT enet_listener (socket4_addr const & saddr);

    /**
     * Constructs POSIX TCP server, bind to the specified address and start
     * listening
     */
    NETTY__EXPORT enet_listener (socket4_addr const & addr, int backlog);

public:
    NETTY__EXPORT native_type native () const noexcept;

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
    NETTY__EXPORT enet_socket accept (error * perr = nullptr);
    NETTY__EXPORT enet_socket accept_nonblocking (error * perr = nullptr);

public:
    NETTY__EXPORT static enet_socket accept (native_type listener_sock
        , error * perr = nullptr);
    NETTY__EXPORT static enet_socket accept_nonblocking (native_type listener_sock
        , error * perr = nullptr);};

}} // namespace netty::enet

