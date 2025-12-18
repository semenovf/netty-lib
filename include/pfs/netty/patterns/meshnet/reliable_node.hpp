    ////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.05.06 Initial version (`reliable_node.cpp`).
//      2025.12.18 Renamed to `reliable_node.hpp`.
//                 `reliable_node` renamed to `reliable_node`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../callback.hpp"
#include "../../trace.hpp"
#include "tag.hpp"
#include <pfs/log.hpp>

#if NETTY__TELEMETRY_ENABLED
#   include "telemetry.hpp"
#endif

NETTY__NAMESPACE_BEGIN

namespace meshnet {

/**
 * Node pool with reliable delivery support
 */
template <typename DeliveryManager>
class reliable_node
{
    using delivery_manager_type = DeliveryManager;
    using transport_type = typename DeliveryManager::transport_type;
    using serializer_traits_type = typename delivery_manager_type::serializer_traits_type;
    using archive_type = typename serializer_traits_type::archive_type;

#if NETTY__TELEMETRY_ENABLED
    using shared_telemetry_producer_type = std::shared_ptr<telemetry_producer<serializer_traits_type>>;
#endif

public:
    using node_id = typename transport_type::node_id;
    using message_id = typename DeliveryManager::message_id;
    using gateway_chain_type = typename transport_type::gateway_chain_type;

private:
    transport_type _t;
    delivery_manager_type _dm;

private:
    callback_t<void (node_id)> _on_node_alive = [] (node_id) {};
    callback_t<void (node_id)> _on_node_unreachable = [] (node_id) {};

private:
    void init ()
    {
        _t.on_node_unreachable([this] (node_id id) {
            NETTY__TRACE(MESHNET_TAG, "node unreachable: {}", to_string(id));
            _dm.pause(id);
            _on_node_unreachable(id);
        }).on_data_received([this] (node_id id, int priority, archive_type bytes) {
            _dm.process_input(id, priority, std::move(bytes));


            // FIXME REMOVE
        })/*.on_node_alive([this] (node_id id) {
            NETTY__TRACE(MESHNET_TAG, "node alive: {}", to_string(id));
            _dm.resume(id);
            _on_node_alive(id);
        })*/;
    }

public:
#if NETTY__TELEMETRY_ENABLED
    reliable_node (node_id id, bool is_gateway, shared_telemetry_producer_type telemetry_producer)
        : _t(id, is_gateway, telemetry_producer)
        , _dm(_t)
    {
        init();
    }
#endif

    reliable_node (node_id id, bool is_gateway = false)
        : _t(id, is_gateway)
        , _dm(_t)
    {
        init();
    }

    reliable_node (reliable_node const &) = delete;
    reliable_node (reliable_node &&) = delete;
    reliable_node & operator = (reliable_node const &) = delete;
    reliable_node & operator = (reliable_node &&) = delete;

    ~reliable_node () = default;

public: // Set callbacks
    /**
     * Sets error callback.
     *
     * @details Callback @a f signature must match:
     *          void (std::string const &)
     */
    template <typename F>
    reliable_node & on_error (F && f)
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
     *          void (peer_index_t, node_id peer_id, bool is_gateway)
     */
    template <typename F>
    reliable_node & on_channel_established (F && f)
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
    reliable_node & on_channel_destroyed (F && f)
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
    reliable_node & on_duplicate_id (F && f)
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
    reliable_node & on_node_alive (F && f)
    {
        _on_node_alive = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when node alive status changed.
     *
     * @details Callback @a f signature must match:
     *          void (node_id)
     */
    template <typename F>
    reliable_node & on_node_unreachable (F && f)
    {
        _on_node_unreachable = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when some route ready by request or response.
     *
     * @details Callback @a f signature must match:
     *          void (node_id dest, gateway_chain_type gw_chain)
     */
    template <typename F>
    reliable_node & on_route_ready (F && f)
    {
        _t.on_route_ready(std::forward<F>(f));
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
    reliable_node & on_receiver_ready (F && f)
    {
        _dm.on_receiver_ready(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify receiver when message received.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, message_id, int priority, archive_type msg)
     */
    template <typename F>
    reliable_node & on_message_received (F && f)
    {
        _dm.on_message_received(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify sender when message delivered to the receiver.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, message_id)
     */
    template <typename F>
    reliable_node & on_message_delivered (F && f)
    {
        _dm.on_message_delivered(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify receiver when message lost while receiving.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, message_id)
     */
    template <typename F>
    reliable_node & on_message_lost (F && f)
    {
        _dm.on_message_lost(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify receiver when report received.
     *
     * @details Callback @a f signature must match:
     *          void (node_id sender, int priority, archive_type report)
     */
    template <typename F>
    reliable_node & on_report_received (F && f)
    {
        _dm.on_report_received(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify receiver about of starting the message receiving.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, message_id, std::size_t total_size)
     */
    template <typename F>
    reliable_node & on_message_receiving_begin (F && f)
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
    reliable_node & on_message_receiving_progress (F && f)
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
    peer_index_t add_node (std::vector<socket4_addr> const & listener_saddrs, error * perr = nullptr)
    {
        return _t.template add_node<Node>(listener_saddrs, perr);
    }

    /**
     * Adds new node to the node pool with specified listeners.
     */
    template <typename Node>
    peer_index_t add_node (std::initializer_list<socket4_addr> const & listener_saddrs, error * perr = nullptr)
    {
        return _t.template add_node<Node>(listener_saddrs, perr);
    }

    void listen (int backlog = 50)
    {
        _t.listen(backlog);
    }

    void listen (peer_index_t index, int backlog)
    {
        _t.listen(index, backlog);
    }

    bool connect_host (peer_index_t index, netty::socket4_addr remote_saddr, bool behind_nat = false)
    {
        return _t.connect_host(index, remote_saddr, behind_nat);
    }

    bool connect_host (peer_index_t index, netty::socket4_addr remote_saddr, netty::inet4_addr local_addr
        , bool behind_nat = false)
    {
        return _t.connect_host(index, remote_saddr, local_addr, behind_nat);
    }

    void disconnect (peer_index_t index, node_id peer_id)
    {
        _t.disconnect(index, peer_id);
    }

    /**
     * Sets maximum frame size @a frame_size for exchange with node specified by
     * identifier @a peer_id.
     */
    void set_frame_size (peer_index_t index, node_id peer_id, std::uint16_t frame_size)
    {
        _t.set_frame_size(index, peer_id, frame_size);
    }

    bool enqueue_message (node_id id, message_id msgid, int priority, archive_type msg)
    {
        return _dm.enqueue_message(id, msgid, priority, std::move(msg));
    }

    bool enqueue_message (node_id id, message_id msgid, int priority, char const * msg
        , std::size_t length)
    {
        return _dm.enqueue_message(id, msgid, priority, msg, length);
    }

    bool enqueue_static_message (node_id id, message_id msgid, int priority, char const * msg
        , std::size_t length)
    {
        return _dm.enqueue_static_message(id, msgid, priority, msg, length);
    }

    bool enqueue_report (node_id id, int priority, char const * data, std::size_t length)
    {
        return _dm.enqueue_report(id, priority, data, length);
    }

    bool enqueue_report (node_id id, int priority, archive_type data)
    {
        return _dm.enqueue_report(id, priority, std::move(data));
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

} // namespace meshnet

NETTY__NAMESPACE_END
