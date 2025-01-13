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
    using listener_id = std::uintptr_t; // _ENetHost *
    using socket_id = std::uintptr_t; // _ENetPeer *
    using peer_socket_type = enet_socket;

private:
    socket4_addr _saddr;
    _ENetHost * _host {nullptr};

public:
    /**
     * Constructs invalid (uninitialized) ENet listener.
     */
    NETTY__EXPORT enet_listener ();

    /**
     * Constructs ENet server and bind to the specified address with backlog equal to 10.
     */
    NETTY__EXPORT enet_listener (socket4_addr const & saddr, netty::error * perr = nullptr);

    NETTY__EXPORT ~enet_listener ();

public:
    /**
     *  Checks if the listener is initialized and already listening
     */
    NETTY__EXPORT operator bool () const noexcept;

    NETTY__EXPORT listener_id id () const noexcept;

    /**
     * Listen for connections on a socket.
     *
     * @param backlog The maximum length to which the queue of pending
     *        connections may grow (will be ignored, set this value in the constructor).
     *
     * @note This method do nothing, only to support unified API.
     */
    NETTY__EXPORT bool listen (int backlog, error * perr = nullptr);

    //NETTY__EXPORT peer_socket_type accept (socket_id peer_id, error * perr = nullptr);

    NETTY__EXPORT peer_socket_type accept_nonblocking (
          listener_id listener_sock // really there is not a listener socket, already accepted socket
        , error * perr = nullptr);
};

}} // namespace netty::enet
