////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/conn_status.hpp>
#include <pfs/netty/error.hpp>
#include <pfs/netty/exports.hpp>
#include <pfs/netty/send_result.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <cstdint>
#include <vector>

struct _ENetHost;
struct _ENetPeer;

namespace netty {
namespace enet {

class enet_listener;

struct uninitialized {};

/**
 * ENet socket
 */
class enet_socket
{
    friend class enet_listener;

public:
    using native_type = std::uintptr_t;

public:
    static const native_type kINVALID_SOCKET;

private:
    _ENetHost * _host {nullptr};
    _ENetPeer * _peer {nullptr};
    std::vector<char> _inpb; // Input buffer

protected:
    /**
     * Constructs ENet accepted socket.
     */
    enet_socket (native_type sock);

    /**
     * Constructs uninitialized (invalid) ENet socket.
     */
    enet_socket (uninitialized);

public:
    enet_socket (enet_socket const & other) = delete;
    enet_socket & operator = (enet_socket const & other) = delete;

    NETTY__EXPORT enet_socket ();
    NETTY__EXPORT enet_socket (enet_socket && other);
    NETTY__EXPORT enet_socket & operator = (enet_socket && other);
    NETTY__EXPORT ~enet_socket ();

public:
    /**
     *  Checks if socket is valid
     */
    NETTY__EXPORT operator bool () const noexcept;

    NETTY__EXPORT native_type native () const noexcept;

    /**
     * @return Listener address for connected socket.
     */
    NETTY__EXPORT socket4_addr saddr () const noexcept;

    NETTY__EXPORT int available (error * perr = nullptr) const;

    NETTY__EXPORT int recv (char * data, int len, error * perr = nullptr);

    /**
     * Send @a data message with @a size on a socket.
     *
     * @param data Data to send.
     * @param size Data size to send.
     * @param overflow Flag that the send buffer is overflow (@c true).
     * @param perr Pointer to structure to store error if occurred.
     */
    NETTY__EXPORT send_result send (char const * data, int len, error * perr = nullptr);

    NETTY__EXPORT int recv_from (char * data, int len, socket4_addr * saddr = nullptr
        , error * perr = nullptr);

    /**
     * See send description.
     */
    NETTY__EXPORT send_result send_to (socket4_addr const & dest, char const * data, int len
        , error * perr = nullptr);

    /**
     * Connects to the ENet server.
     *
     * @return @c conn_status::failure if error occurred while connecting,
     *         @c conn_status::success if connection established successfully or
     *         @c conn_status::in_progress if connection in progress.
     */
    NETTY__EXPORT conn_status connect (socket4_addr const & saddr, error * perr = nullptr);

    /**
     * Shutdown connection.
     */
    NETTY__EXPORT void disconnect (error * perr = nullptr);
};

}} // namespace netty::enet

