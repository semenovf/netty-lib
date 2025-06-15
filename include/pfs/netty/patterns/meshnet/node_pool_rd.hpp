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

public:
    using node_id = typename transport_type::node_id;
    using message_id = typename DeliveryManager::message_id;
    using gateway_chain_type = typename transport_type::gateway_chain_type;

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
    mutable callback_t<void (node_id, socket4_addr)> on_duplicate_id
        = [] (node_id, socket4_addr) {};

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
    mutable callback_t<void (node_id, gateway_chain_type)> on_route_ready
        = [] (node_id /*dest*/, gateway_chain_type /*gw_chain*/) {};

    /**
     * Notify sender when data actually sent (written into the socket).
     */
    mutable callback_t<void (node_id, std::uint64_t)> on_bytes_written
        = [] (node_id, std::uint64_t /*n*/) {};

    /**
     * Notify sender when synchronization with the receiver is complete (logical reliable delivery
     * channel established).
     */
    mutable callback_t<void (node_id)> on_receiver_ready = [] (node_id) {};

    /**
     * Notify receiver when message received.
     */
    mutable callback_t<void (node_id, message_id, std::vector<char>)> on_message_received
        = [] (node_id, message_id, std::vector<char>) {};

    /**
     * Notify sender when message delivered to the receiver.
     */
    mutable callback_t<void (node_id, message_id)> on_message_delivered
        = [] (node_id, message_id) {};

    /**
     * Notify receiver when report received.
     */
    mutable callback_t<void (node_id, std::vector<char>)> on_report_received
        = [] (node_id, std::vector<char>) {};

public:
    node_pool_rd (node_id id, bool is_gateway = false)
        : _t(id, is_gateway)
        , _dm(_t)
    {
        _t.on_error = _dm.on_error = [this] (std::string const & errstr)
        {
            this->on_error(errstr);
        };

        //
        // Transport specific callbacks.
        //
        _t.on_channel_established = [this] (node_id id, bool is_gateway)
        {
            this->on_channel_established(id, is_gateway);
        };

        _t.on_channel_destroyed = [this] (node_id id)
        {
            this->on_channel_destroyed(id);
        };

        _t.on_duplicate_id = [this] (node_id id, socket4_addr saddr)
        {
            this->on_duplicate_id(id, saddr);
        };

        _t.on_node_alive = [this] (node_id id)
        {
            this->on_node_alive(id);
        };

        _t.on_node_expired = [this] (node_id id)
        {
            this->on_node_expired(id);
        };

        _t.on_route_ready = [this] (node_id dest_id, gateway_chain_type gw_chain)
        {
            this->on_route_ready(dest_id, std::move(gw_chain));
        };

        _t.on_bytes_written = [this] (node_id id, std::uint64_t n)
        {
            this->on_bytes_written(id, n);
        };

        _t.on_data_received = [this] (node_id id, int priority, std::vector<char> bytes)
        {
            _dm.process_packet(id, priority, std::move(bytes));
        };

        //
        // Reliable delivery managr specific callbacks.
        //
        _dm.on_receiver_ready = [this] (node_id id)
        {
            this->on_receiver_ready(id);
        };

        _dm.on_message_received = [this] (node_id id, message_id msgid, std::vector<char> msg)
        {
            this->on_message_received(id, msgid, std::move(msg));
        };

        _dm.on_message_delivered = [this] (node_id id, message_id msgid)
        {
            this->on_message_delivered(id, msgid);
        };

        _dm.on_report_received = [this] (node_id id, std::vector<char> report)
        {
            this->on_report_received(id, std::move(report));
        };
    }

    node_pool_rd (node_pool_rd const &) = delete;
    node_pool_rd (node_pool_rd &&) = delete;
    node_pool_rd & operator = (node_pool_rd const &) = delete;
    node_pool_rd & operator = (node_pool_rd &&) = delete;

    ~node_pool_rd () = default;

public:
    node_id id () const noexcept
    {
        return _t.id();
    }

    std::string name () const noexcept
    {
        return _t.name();
    }

    bool is_gateway () const noexcept
    {
        return _t.is_gateway();
    }

    template <typename Node>
    node_index_t add_node (std::vector<socket4_addr> const & listener_saddrs, error * perr = nullptr)
    {
        return _t.template add_node<Node>(listener_saddrs, perr);
    }

    /**
     * Adds new node to the node pool with specified listeners.
     */
    template <typename Node>
    node_index_t add_node (std::initializer_list<socket4_addr> const & listener_saddrs, error * perr = nullptr)
    {
        return _t.template add_node<Node>(listener_saddrs, perr);
    }

    void listen (int backlog = 50)
    {
        _t.listen(backlog);
    }

    void listen (node_index_t index, int backlog)
    {
        _t.listen(index, backlog);
    }

    bool connect_host (node_index_t index, netty::socket4_addr remote_saddr, bool behind_nat = false)
    {
        return _t.connect_host(index, remote_saddr, behind_nat);
    }

    bool connect_host (node_index_t index, netty::socket4_addr remote_saddr, netty::inet4_addr local_addr
        , bool behind_nat = false)
    {
        return _t.connect_host(index, remote_saddr, local_addr, behind_nat);
    }

    bool enqueue_message (node_id id, message_id msgid, int priority, bool force_checksum
        , std::vector<char> msg)
    {
        return _dm.enqueue_message(id, msgid, priority, force_checksum, std::move(msg));
    }

    bool enqueue_message (node_id id, message_id msgid, int priority, bool force_checksum
        , char const * msg, std::size_t length)
    {
        return _dm.enqueue_message(id, msgid, priority, force_checksum, msg, length);
    }

    bool enqueue_static_message (node_id id, message_id msgid, int priority
        , bool force_checksum, char const * msg, std::size_t length)
    {
        return _dm.enqueue_static_message(id, msgid, priority, force_checksum, msg, length);
    }

    bool enqueue_report (node_id id, int priority, bool force_checksum, char const * data
        , std::size_t length)
    {
        return _dm.enqueue_report(id, priority, force_checksum, data, length);
    }

    bool enqueue_report (node_id id, int priority, bool force_checksum, std::vector<char> data)
    {
        return _dm.enqueue_report(id, priority, force_checksum, std::move(data));
    }

    void interrupt ()
    {
        _t.interrupt();
    }

    bool interrupted () const noexcept
    {
        return _t.interrupted();
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step ()
    {
        return _dm.step();
    }

    void run (std::chrono::milliseconds loop_interval = std::chrono::milliseconds{10})
    {
        _t.clear_interrupted();

        while (!interrupted()) {
            pfs::countdown_timer<std::milli> countdown_timer {loop_interval};
            auto n = step();

            if (n == 0)
                std::this_thread::sleep_for(countdown_timer.remain());
        }
    }

public: // Set optional callbacks
    /**
     * Notify receiver about message receiving progress.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, message_id, std::size_t received_size, std::size_t total_size)
     */
    template <typename F>
    node_pool_rd & on_message_receiving_progress (F && f)
    {
        _dm.on_message_receiving_progress = std::forward<F>(f);
        return *this;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
