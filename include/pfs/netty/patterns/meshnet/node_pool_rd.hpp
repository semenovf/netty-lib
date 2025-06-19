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
    node_pool_rd (node_id id, bool is_gateway = false)
        : _t(id, is_gateway)
        , _dm(_t)
    {
        _t.on_data_received([this] (node_id id, int priority, std::vector<char> bytes)
        {
            _dm.process_packet(id, priority, std::move(bytes));
        });
    }

    node_pool_rd (node_pool_rd const &) = delete;
    node_pool_rd (node_pool_rd &&) = delete;
    node_pool_rd & operator = (node_pool_rd const &) = delete;
    node_pool_rd & operator = (node_pool_rd &&) = delete;

    ~node_pool_rd () = default;

public: // Set callbacks
    /**
     * Sets error callback.
     *
     * @details Callback @a f signature must match:
     *          void (std::string const &)
     */
    template <typename F>
    node_pool_rd & on_error (F && f)
    {
        _t.on_error(f);
        _dm.on_error(std::forward<F>(f));
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Transport specific callbacks.
    ////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Notify when connection established with the remote node.
     *
     * @details Callback @a f signature must match:
     *          void (node_id id, bool is_gateway)
     */
    template <typename F>
    node_pool_rd & on_channel_established (F && f)
    {
        _t.on_channel_established(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify when the channel is destroyed with the remote node.
     *
     * @details Callback @a f signature must match:
     *          void (node_id id)
     */
    template <typename F>
    node_pool_rd & on_channel_destroyed (F && f)
    {
        _t.on_channel_destroyed(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify when a node with identical ID is detected.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, socket4_addr)
     */
    template <typename F>
    node_pool_rd & on_duplicate_id (F && f)
    {
        _t.on_duplicate_id(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify when node alive status changed.
     *
     * @details Callback @a f signature must match:
     *          void (node_id)
     */
    template <typename F>
    node_pool_rd & on_node_alive (F && f)
    {
        _t.on_node_alive(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify when node alive status changed.
     *
     * @details Callback @a f signature must match:
     *          void (node_id)
     */
    template <typename F>
    node_pool_rd & on_node_expired (F && f)
    {
        _t.on_node_expired(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify when some route ready by request or response.
     *
     * @details Callback @a f signature must match:
     *          void (node_id dest, gateway_chain_type gw_chain)
     */
    template <typename F>
    node_pool_rd & on_route_ready (F && f)
    {
        _t.on_route_ready(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify sender when data actually sent (written into the socket).
     *
     * @details Callback @a f signature must match:
     *          void (node_id receiver, std::uint64_t bytes_written_size)
     */
    template <typename F>
    node_pool_rd & on_bytes_written (F && f)
    {
        _t.on_bytes_written(std::forward<F>(f));
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Reliable delivery manager specific callbacks.
    ////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Notify sender when synchronization with the receiver is complete (logical reliable delivery
     * channel established).
     *
     * @details Callback @a f signature must match:
     *          void (node_id)
     */
    template <typename F>
    node_pool_rd & on_receiver_ready (F && f)
    {
        _dm.on_receiver_ready(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify receiver when message received.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, message_id, int priority, std::vector<char> msg)
     */
    template <typename F>
    node_pool_rd & on_message_received (F && f)
    {
        _dm.on_message_received((std::forward<F>(f)));
        return *this;
    }

    /**
     * Notify sender when message delivered to the receiver.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, message_id)
     */
    template <typename F>
    node_pool_rd & on_message_delivered (F && f)
    {
        _dm.on_message_delivered((std::forward<F>(f)));
        return *this;
    }

    /**
     * Notify receiver when report received.
     *
     * @details Callback @a f signature must match:
     *          void (node_id sender, int priority, std::vector<char> report)
     */
    template <typename F>
    node_pool_rd & on_report_received (F && f)
    {
        _dm.on_report_received((std::forward<F>(f)));
        return *this;
    }

    /**
     * Notify receiver about of starting the message receiving.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, message_id, std::size_t total_size)
     */
    template <typename F>
    node_pool_rd & on_message_receiving_begin (F && f)
    {
        _dm.on_message_receiving_begin(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify receiver about message receiving progress.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, message_id, std::size_t received_size, std::size_t total_size)
     */
    template <typename F>
    node_pool_rd & on_message_receiving_progress (F && f)
    {
        _dm.on_message_receiving_progress(std::forward<F>(f));
        return *this;
    }

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
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
