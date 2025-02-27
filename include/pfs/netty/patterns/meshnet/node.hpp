////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "channel_interface.hpp"
#include "functional_callbacks.hpp"
#include <pfs/countdown_timer.hpp>
#include <pfs/i18n.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdTraits
    , typename RoutingTable
    , typename Loggable>
class node: public Loggable
{
public:
    using node_id_traits = NodeIdTraits;
    using node_id = typename NodeIdTraits::node_id;

private:
    using channel_interface_type = channel_interface<NodeIdTraits>;
    using channel_interface_ptr = std::unique_ptr<channel_interface_type>;
    using routing_table_type = RoutingTable;

private:
    node_id _id;

    // True if the node is behind NAT
    bool _behind_nat {false};

    // There will rarely be more than two channels, so vector is a optimal choice
    std::vector<channel_interface_ptr> _channels;

    routing_table_type _rtab;

    std::shared_ptr<functional_channel_callbacks<node_id>> _channel_callbacks;

public:
    node (node_id id, bool behind_nat)
        : Loggable()
        , _id(id)
        , _behind_nat(behind_nat)
    {
        _channel_callbacks = std::make_shared<functional_channel_callbacks<node_id>>();

        _channel_callbacks->on_channel_established = [] (node_id id) {};
        _channel_callbacks->on_channel_destroyed = [] (node_id id) {};
        _channel_callbacks->on_bytes_written = [] (node_id, std::uint64_t /*n*/) {};

        _channel_callbacks->on_route_received = [this] (node_id, bool is_response
            , std::vector<std::pair<std::uint64_t, std::uint64_t>> && route) {
            _rtab.process(is_response, std::move(route));
        };

        _channel_callbacks->on_message_received = [] (node_id, std::vector<char> && /*bytes*/) {};
        _channel_callbacks->on_foreign_message_received = [] (node_id
            , std::pair<std::uint64_t, std::uint64_t> /*sender_id*/
            , std::pair<std::uint64_t, std::uint64_t> /*receiver_id*/
            , std::vector<char> && /*bytes*/) {};
    }

    node (node const &) = delete;
    node (node &&) = delete;
    node & operator = (node const &) = delete;
    node & operator = (node &&) = delete;

    ~node () = default;

private:
    channel_interface_type * locate_writer (node_id id)
    {
        if (_channels.empty())
            return nullptr;

        // TODO First check routing table
        //...

        if (_channels[0]->has_writer(id)) {
            // TODO Update routing table
            // ...

            return & *_channels[0];
        }

        for (int i = 1; i < _channels.size(); i++) {
            if (_channels[i]->has_writer(id)) {
                // TODO Update routing table
                // ...

                return & *_channels[i];
            }
        }

        return nullptr;
    }

public:
    /**
     * Adds new channel to the node.
     */
    template <typename Channel>
    channel_interface_type & add_channel ()
    {
        static_assert(std::is_same<typename Channel::callback_suite, functional_channel_callbacks<node_id>>::value
            , "Incompatible callback for node");

        auto x = Channel::template make_interface(_id, _behind_nat, _channel_callbacks);
        _channels.push_back(std::move(x));
        return *_channels.back();
    }

    /**
     * Equeues message for delivery to specified node ID @a id.
     *
     * @param id Receiver ID.
     * @param priority Message priority.
     * @param force_checksum Calculate checksum before sending.
     * @param data Message content.
     * @param len Length of the message content.
     *
     * @return @c true if message route found for @a id.
     */
    bool enqueue (node_id id, int priority, bool force_checksum, char const * data, std::size_t len)
    {
        auto channel_ptr = locate_writer(id);

        if (channel_ptr == nullptr) {
            this->log_error(tr::f_("node not found to send message: {}", NodeIdTraits::stringify(id)));
            return false;
        }

        channel_ptr->enqueue(id, priority, force_checksum, data, len);
        return true;
    }

    /**
     * Equeues message for delivery to specified node ID @a id.
     *
     * @param id Receiver ID.
     * @param priority Message priority.
     * @param data Message content.
     * @param len Length of the message content.
     *
     * @return @c true if message route found for @a id.
     */
    bool enqueue (node_id id, int priority, char const * data, std::size_t len)
    {
        return enqueue(id, priority, false, data, len);
    }

    void listen (int backlog = 50)
    {
        for (auto & x: _channels)
            x->listen(backlog);
    }

    void step (std::chrono::milliseconds millis)
    {
        pfs::countdown_timer<std::milli> countdown_timer {millis};

        for (auto & x: _channels)
            x->step();

        std::this_thread::sleep_for(countdown_timer.remain());
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
