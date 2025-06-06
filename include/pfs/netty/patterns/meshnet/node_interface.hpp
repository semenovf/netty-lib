////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.25 Initial version.
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

namespace patterns {
namespace meshnet {

template <typename NodeId>
class node_interface
{
public:
    using node_id = NodeId;

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
    virtual void enqueue (node_id id, int priority, bool force_checksum, char const * data, std::size_t len) = 0;
    virtual void enqueue (node_id id, int priority, bool force_checksum, std::vector<char> data) = 0;
    virtual bool has_writer (node_id id) const = 0;
    virtual unsigned int step () = 0;
    virtual void clear_channels () = 0;

    //
    // Callback assign methods
    //
    virtual void on_error (callback_t<void (std::string const &)>) = 0;
    virtual void on_channel_established (callback_t<void (node_id, node_index_t, bool /*is_gateway*/)>) = 0;
    virtual void on_channel_destroyed (callback_t<void (node_id, node_index_t)>) = 0;
    virtual void on_duplicate_id (callback_t<void (node_id, node_index_t, socket4_addr)>) = 0;
    virtual void on_bytes_written (callback_t<void (node_id, std::uint64_t)>) = 0;
    virtual void on_alive_received (callback_t<void (node_id, node_index_t, alive_info<node_id> const &)>) = 0;
    virtual void on_unreachable_received (callback_t<void (node_id, node_index_t, unreachable_info<node_id> const &)>) = 0;
    virtual void on_route_received (callback_t<void (node_id, node_index_t, bool, route_info<node_id> const &)>) = 0;
    virtual void on_domestic_data_received (callback_t<void (node_id, int, std::vector<char>)>) = 0;
    virtual void on_global_data_received (callback_t<void (node_id /*last transmitter node*/
        , int /*priority*/, node_id /*sender ID*/, node_id /*receiver ID*/, std::vector<char>)>) = 0;
    virtual void on_forward_global_packet (callback_t<void (int /*priority*/, node_id /*sender ID*/
        , node_id /*receiver ID*/, std::vector<char>)>) = 0;

    //
    // For internal use only
    //
    virtual bool enqueue_packet (node_id id, int priority, std::vector<char> data) = 0;
    virtual bool enqueue_packet (node_id id, int priority, char const * data, std::size_t len) = 0;
    virtual void enqueue_broadcast_packet (int priority, char const * data, std::size_t len) = 0;
    virtual void enqueue_forward_packet (node_id sender_id, int priority
        , char const * data, std::size_t len) = 0;
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
