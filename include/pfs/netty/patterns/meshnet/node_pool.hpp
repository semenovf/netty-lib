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
#include "node_interface.hpp"
#include "functional_callbacks.hpp"
#include <pfs/countdown_timer.hpp>
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
class node_pool: public Loggable
{
public:
    using node_id = typename NodeIdTraits::node_id;

private:
    using node_interface_type = node_interface<NodeIdTraits>;
    using node_interface_ptr = std::unique_ptr<node_interface_type>;
    using routing_table_type = RoutingTable;

private:
    node_id _id;
    // True if the node is behind NAT
    bool _behind_nat {false};

    // There will rarely be more than two nodes, so vector is a optimal choice
    std::vector<node_interface_ptr> _nodes;

    routing_table_type _rtable;

    std::shared_ptr<functional_node_callbacks<node_id>> _node_callbacks;

public:
    node_pool (node_id id, bool behind_nat)
        : Loggable()
        , _id(id)
        , _behind_nat(behind_nat)
    {
        _node_callbacks = std::make_shared<functional_node_callbacks<node_id>>();

        _node_callbacks->on_node_connected = [] (node_id id) {};
        _node_callbacks->on_node_disconnected = [] (node_id id) {};
        _node_callbacks->on_bytes_written = [] (node_id, std::uint64_t /*n*/) {};
        _node_callbacks->on_route_received = [] (node_id, bool /*is_response*/
            , std::vector<std::pair<std::uint64_t, std::uint64_t>> && /*route*/) {};
        _node_callbacks->on_message_received = [] (node_id, std::vector<char> && /*bytes*/) {};
        _node_callbacks->on_foreign_message_received = [] (node_id
            , std::pair<std::uint64_t, std::uint64_t> /*sender_id*/
            , std::pair<std::uint64_t, std::uint64_t> /*receiver_id*/
            , std::vector<char> && /*bytes*/) {};
    }

    node_pool (node_pool const &) = delete;
    node_pool (node_pool &&) = delete;
    node_pool & operator = (node_pool const &) = delete;
    node_pool & operator = (node_pool &&) = delete;

    ~node_pool () = default;

private:
    node_interface_type * locate_node (node_id id)
    {
        if (_nodes.empty())
            return nullptr;

        // if (_nodes[0]->id() == id)
        //     return & *_nodes[0];
        //
        // for (int i = 1; i < _nodes.size(); i++) {
        //     if (_nodes[i]->id() == id)
        //         return & *_nodes[i];
        // }

        // FIXME Select node from routing table
        return nullptr;
    }

public:
    template <typename Node>
    node_interface_type & add_node ()
    {
        static_assert(std::is_same<typename Node::callback_suite, functional_node_callbacks<node_id>>::value
            , "Incompatible callback for node");

        auto x = Node::template make_interface(_id, _behind_nat, _node_callbacks);
        _nodes.push_back(std::move(x));
        return *_nodes.back();
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
        auto node_ptr = locate_node(id);

        if (node_ptr == nullptr)
            return false;

        node_ptr->enqueue(id, priority, force_checksum, data, len);
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
        for (auto & x: _nodes)
            x->listen(backlog);
    }

    void step (std::chrono::milliseconds millis)
    {
        pfs::countdown_timer<std::milli> countdown_timer {millis};

        for (auto & x: _nodes)
            x->step();

        std::this_thread::sleep_for(countdown_timer.remain());
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
