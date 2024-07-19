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

struct _ENetHost;
struct _ENetPeer;

namespace netty {
namespace enet {

class enet_listener
{
public:
    using native_type = std::uintptr_t; // _ENetHost *
    using native_socket_type = std::uintptr_t; // _ENetPeer *

private:
    socket4_addr _saddr;
    _ENetHost * _host {nullptr};

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
    NETTY__EXPORT enet_listener (socket4_addr const & saddr, int backlog);

    NETTY__EXPORT ~enet_listener ();

public:
    NETTY__EXPORT native_type native () const noexcept;

    /**
     * Listen for connections on a socket.
     *
     * @param backlog The maximum length to which the queue of pending
     *        connections may grow.
     */
    NETTY__EXPORT bool listen (int backlog, error * perr = nullptr);

public:
    NETTY__EXPORT static enet_socket accept (
          native_type listener_sock // really here not a listener socket, already accepted socket
        , error * perr = nullptr);

    NETTY__EXPORT static enet_socket accept_nonblocking (
          native_type listener_sock // really here not a listener socket, already accepted socket
        , error * perr = nullptr);
};

}} // namespace netty::enet
