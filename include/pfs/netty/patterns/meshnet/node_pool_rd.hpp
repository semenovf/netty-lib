    ////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.05.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../callback.hpp"
#include <pfs/log.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

/**
 * Node pool with reliable delivery support
 */
template <typename DeliveryManager>
class node_pool_rd
{
    using delivery_manager_type = DeliveryManager;
    using transport_type = typename DeliveryManager::transport_type;
    using node_id = typename transport_type::node_id;
    using message_id = typename DeliveryManager::message_id;

private:
    transport_type _t;
    delivery_manager_type _dm;

public:
    /**
     * Notify when error occurred.
     */
    mutable callback_t<void (std::string const &)> on_error
        = [] (std::string const & errstr) { LOGE(TAG, "{}", errstr); };

    /**
     * Notify when connection established with the remote node.
     */
    mutable callback_t<void (node_id, bool)> on_channel_established
        = [] (node_id, bool /*is_gateway*/) {};

    /**
     * Notify when the channel is destroyed with the remote node.
     */
    mutable callback_t<void (node_id)> on_channel_destroyed = [] (node_id) {};

    /**
     * Notify when a node with identical ID is detected.
     */
    mutable callback_t<void (node_id, std::string const &, socket4_addr)> on_duplicated
        = [] (node_id, std::string const & /*name*/, socket4_addr) {};

    /**
     * Notify when node alive status changed.
     */
    mutable callback_t<void (node_id)> on_node_alive = [] (node_id) {};

    /**
     * Notify when node alive status changed.
     */
    mutable callback_t<void (node_id)> on_node_expired = [] (node_id) {};

    /**
     * Notify when some route ready by request or response.
     */
    mutable callback_t<void (node_id, std::uint16_t)> on_route_ready
        = [] (node_id /*dest*/, std::uint16_t /*hops*/) {};

    /**
     * Notify when data actually sent (written into the socket).
     */
    mutable callback_t<void (node_id, std::uint64_t)> on_bytes_written
        = [] (node_id, std::uint64_t /*n*/) {};

    /**
     * Notify when synchronization with the receiver is complete (reliable delivery channel
     * established).
     */
    mutable callback_t<void (node_id)> on_receiver_ready = [] (node_id) {};

    /**
     * Notify when message received.
     */
    mutable callback_t<void (node_id, message_id, std::vector<char>)> on_message_received
        = [] (node_id, message_id, std::vector<char>) {};

    /**
     * Notify when message delivered to the receiver.
     */
    mutable callback_t<void (node_id, message_id)> on_message_delivered
        = [] (node_id, message_id) {};

    /**
     * Notify when report received.
     */
    mutable callback_t<void (node_id, std::vector<char>)> on_report_received
        = [] (node_id, std::vector<char>) {};

public:
    node_pool_rd (node_id id, std::string name, bool is_gateway = false)
        : _t(id, name, is_gateway)
        , _dm(_t)
    {
        _t.on_error = _dm.on_error = [this] (std::string const & errstr)
        {
            this->on_error(errstr);
        };
    }

    node_pool_rd (node_pool_rd const &) = delete;
    node_pool_rd (node_pool_rd &&) = delete;
    node_pool_rd & operator = (node_pool_rd const &) = delete;
    node_pool_rd & operator = (node_pool_rd &&) = delete;

    ~node_pool_rd () = default;

private:
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
