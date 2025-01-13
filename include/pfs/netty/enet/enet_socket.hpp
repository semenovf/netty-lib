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
#include <pfs/netty/uninitialized.hpp>
#include <cstdint>
#include <vector>

struct _ENetHost;
struct _ENetPeer;

namespace netty {
namespace enet {

class enet_listener;

/**
 * ENet socket
 */
class enet_socket
{
    friend class enet_listener;

public:
    using input_buffer_type = std::vector<char>;

    enum net_quality {
          defaults // ENet default timeouts for peer
        , fast
        , normal
        , poor
    };

    // _ENetPeer *
    using socket_id = std::uintptr_t;

public:
    static NETTY__EXPORT const socket_id kINVALID_SOCKET;

private:
    _ENetHost * _host {nullptr};
    _ENetPeer * _peer {nullptr};
    int _timeout_limit {0};
    int _timeout_min {0};
    int _timeout_max {0};

    bool _accepted_socket {false};

    // Input buffer. Set by the listener in accept procedure or when connected if this
    // is a client socket.
    input_buffer_type _inpb;

protected:
    /**
     * Constructs ENet accepted socket.
     */
    enet_socket (_ENetHost * host, _ENetPeer * peer) noexcept;

    /**
     * Constructs ENet socket with specified properties. Accepts the following parameters:
     *      - @a timeout_limit - the timeout limit in milliseconds;
     *      - @a timeout_min - the timeout minimum in milliseconds;
     *      - @a timeout_max - the timeout maximum in milliseconds.
     *
     * The timeout parameter control how and when a peer will timeout from a failure to acknowledge
     * reliable traffic. Timeout values use an exponential backoff mechanism, where if a reliable
     * packet is not acknowledge within some multiple of the average RTT plus a variance tolerance,
     * the timeout will be doubled until it reaches a set limit. If the timeout is thus at this
     * limit and reliable packets have been sent but not acknowledged within a certain minimum time
     * period, the peer will be disconnected. Alternatively, if reliable packets have been sent
     * but not acknowledged for a certain maximum time period, the peer will be disconnected regardless
     * of the current timeout limit value.
     */
    void init (int timeout_limit, int timeout_min, int timeout_max, error * perr = nullptr);

public:
    enet_socket (enet_socket const & other) = delete;
    enet_socket & operator = (enet_socket const & other) = delete;

    /**
     * Constructs uninitialized (invalid) ENet socket.
     */
    NETTY__EXPORT enet_socket (uninitialized);

    NETTY__EXPORT enet_socket (net_quality nq = net_quality::normal, error * perr = nullptr);

    NETTY__EXPORT enet_socket (enet_socket && other) noexcept;
    NETTY__EXPORT enet_socket & operator = (enet_socket && other) noexcept;
    NETTY__EXPORT ~enet_socket ();

public:
    /**
     *  Checks if socket is valid
     */
    NETTY__EXPORT operator bool () const noexcept;

    NETTY__EXPORT socket_id id () const noexcept;

    /**
     * @return Peer address for connected socket.
     */
    NETTY__EXPORT socket4_addr saddr () const noexcept;

    // TODO DEPRECATED
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
