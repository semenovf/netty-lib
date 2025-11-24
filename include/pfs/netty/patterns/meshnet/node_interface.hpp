////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.25 Initial version.
//      2025.06.30 Added method `set_frame_size()`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../callback.hpp"
#include "../../error.hpp"
#include "../../socket4_addr.hpp"
#include "node_index.hpp"
#include <chrono>
#include <string>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

template <typename NodeId, typename Archive>
class node_interface
{
public:
    using node_id = NodeId;
    using archive_type = Archive;

public:
    virtual ~node_interface () {}

public:
    virtual node_id id () const noexcept = 0;

    virtual void set_index (node_index_t index) noexcept = 0;
    virtual node_index_t index () const noexcept = 0;

    virtual void add_listener (netty::socket4_addr const & listener_addr, error * perr = nullptr) = 0;
    virtual bool connect_host (netty::socket4_addr remote_saddr, bool behind_nat = false) = 0;
    virtual bool connect_host (netty::socket4_addr remote_saddr, netty::inet4_addr local_addr, bool behind_nat) = 0;
    virtual void listen (int backlog = 50) = 0;
    virtual void enqueue (node_id id, int priority, char const * data, std::size_t len) = 0;
    virtual void enqueue (node_id id, int priority, archive_type data) = 0;
    virtual bool has_writer (node_id id) const = 0;
    virtual void set_frame_size (node_id id, std::uint16_t frame_size) = 0  ;
    virtual unsigned int step () = 0;
    virtual void clear_channels () = 0;

    //
    // Callback assign methods
    //
    virtual void on_error (callback_t<void (std::string const &)>) = 0;
    virtual void on_channel_established (callback_t<void (node_id, node_index_t, bool /*is_gateway*/)>) = 0;
    virtual void on_channel_destroyed (callback_t<void (node_id, node_index_t)>) = 0;
    virtual void on_reconnection_started (callback_t<void (node_index_t, socket4_addr, inet4_addr)>) = 0;
    virtual void on_reconnection_stopped (callback_t<void (node_index_t, socket4_addr, inet4_addr)>) = 0;
    virtual void on_duplicate_id (callback_t<void (node_id, node_index_t, socket4_addr)>) = 0;
    virtual void on_alive_received (callback_t<void (node_id, node_index_t, alive_info<node_id> const &)>) = 0;
    virtual void on_unreachable_received (callback_t<void (node_id, node_index_t, unreachable_info<node_id> const &)>) = 0;
    virtual void on_route_received (callback_t<void (node_id, node_index_t, bool, route_info<node_id> const &)>) = 0;
    virtual void on_domestic_data_received (callback_t<void (node_id, int, archive_type)>) = 0;
    virtual void on_global_data_received (callback_t<void (node_id /*last transmitter node*/
        , int /*priority*/, node_id /*sender ID*/, node_id /*receiver ID*/, archive_type)>) = 0;
    virtual void on_forward_global_packet (callback_t<void (int /*priority*/, node_id /*sender ID*/
        , node_id /*receiver ID*/, archive_type)>) = 0;

    //
    // For internal use only
    //
    virtual bool enqueue_packet (node_id id, int priority, archive_type data) = 0;
    virtual bool enqueue_packet (node_id id, int priority, char const * data, std::size_t len) = 0;
    virtual void enqueue_broadcast_packet (int priority, char const * data, std::size_t len) = 0;
    virtual void enqueue_forward_packet (node_id sender_id, int priority
        , char const * data, std::size_t len) = 0;
};

} // namespace meshnet

NETTY__NAMESPACE_END
