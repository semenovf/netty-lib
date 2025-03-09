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
#include "behind_nat_enum.hpp"
#include "functional_callbacks.hpp"
#include "gateway_enum.hpp"
#include "node_index.hpp"
#include "node_interface.hpp"
#include "route_info.hpp"
#include <pfs/assert.hpp>
#include <pfs/countdown_timer.hpp>
#include <pfs/i18n.hpp>
#include <pfs/numeric_cast.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

#include <pfs/log.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdTraits
    , typename RoutingTable
    , typename CallbackSuite
    , typename Loggable>
class node_pool: public Loggable
{
public:
    using node_id_traits = NodeIdTraits;
    using node_id = typename NodeIdTraits::node_id;
    using callback_suite = CallbackSuite;

private:
    using node_interface_type = node_interface<NodeIdTraits>;
    using node_interface_ptr = std::unique_ptr<node_interface_type>;
    using routing_table_type = RoutingTable;

private:
    node_id _id;
    std::pair<std::uint64_t, std::uint64_t> _id_pair; // Node ID representation

    // True if the node is behind NAT
    bool _behind_nat {false};

    // True if the node is a gateway
    bool _is_gateway {false};

    // There will rarely be more than two nodes, so vector is a optimal choice
    std::vector<node_interface_ptr> _nodes;

    routing_table_type _rtab;
    std::shared_ptr<callback_suite> _callbacks;

    std::shared_ptr<node_callbacks<node_id>> _node_callbacks;

public:
    node_pool (node_id id, behind_nat_enum behind_nat, gateway_enum is_gateway
        , std::shared_ptr<callback_suite> callbacks)
        : Loggable()
        , _id(id)
        , _behind_nat(behind_nat == behind_nat_enum::yes)
        , _is_gateway(is_gateway == gateway_enum::yes)
        , _rtab(id)
        , _callbacks(callbacks)
    {
        _id_pair.first = node_id_traits::high(_id);
        _id_pair.second = node_id_traits::low(_id);

        _node_callbacks = std::make_shared<node_callbacks<node_id>>();

        _node_callbacks->on_channel_established = [this] (node_id id, bool is_gateway) {
            // Start routes discovery if channel established with gateway
            if (is_gateway) {
                _rtab.append_gateway(id);
                std::vector<char> msg = _rtab.build_route_request();
                this->enqueue_packet(id, 0, std::move(msg));
            }

            // Add direct route
            _rtab.add_route(id, node_id{}, 0);

            _callbacks->on_channel_established(id, is_gateway);
        };

        _node_callbacks->on_channel_destroyed = [this] (node_id id) {
            _callbacks->on_channel_destroyed(id);
        };

        _node_callbacks->on_bytes_written = [this] (node_id id, std::uint64_t n) {
            _callbacks->on_bytes_written(id, n);
        };

        _node_callbacks->on_route_received = [this] (node_id id, bool is_response, route_info const & rinfo) {
            process_route_received(id, is_response, rinfo);
        };

        _node_callbacks->on_domestic_message_received = [this] (node_id id, int priority, std::vector<char> && bytes) {
            _callbacks->on_message_received(id, priority, std::move(bytes));
        };

        _node_callbacks->on_global_message_received = [this] (node_id /*id*/, int priority
                , node_id sender_id, node_id receiver_id, std::vector<char> && bytes) {

            PFS__TERMINATE(_id == receiver_id, "Fix meshnet::node_pool algorithm");
            _callbacks->on_message_received(sender_id, priority, std::move(bytes));
        };

        _node_callbacks->on_forward_global_message = [this] (int priority
                , node_id /*sender_id*/, node_id receiver_id, std::vector<char> && packet) {

            PFS__TERMINATE(_id != receiver_id && _is_gateway, "Fix meshnet::node_pool algorithm");
            enqueue_packet(receiver_id, priority, std::move(packet));
        };
    }

    node_pool (node_pool const &) = delete;
    node_pool (node_pool &&) = delete;
    node_pool & operator = (node_pool const &) = delete;
    node_pool & operator = (node_pool &&) = delete;

    ~node_pool () = default;

private:
    node_interface_type * locate_writer (node_id id)
    {
        if (_nodes.empty())
            return nullptr;

        auto gwid = _rtab.gateway_for(id);

        if (_nodes[0]->has_writer(gwid))
            return & *_nodes[0];

        for (int i = 1; i < _nodes.size(); i++) {
            if (_nodes[i]->has_writer(gwid)) {
                return & *_nodes[i];
            }
        }

        return nullptr;
    }

    node_interface_type * locate_node (node_index_t index)
    {
        if (index < 1 && index > _nodes.size()) {
            this->log_error(tr::f_("node index is out of bounds: {}", index));
            return nullptr;
        }

        return & *_nodes[index - 1];
    }

    /**
     * Searches node index that contains specified node
     */
    node_index_t find_node_index (node_id id) const
    {
        if (_nodes.empty())
            return INVALID_NODE_INDEX;

        if (_nodes[0]->has_writer(id))
            return node_index_t{1};

        for (node_index_t i = 1; i < _nodes.size(); i++) {
            if (_nodes[i]->has_writer(id))
                return i + 1;
        }

        return INVALID_NODE_INDEX;
    }

    bool enqueue_packet (node_id id, int priority, std::vector<char> && data)
    {
        auto ptr = locate_writer(id);

        if (ptr == nullptr) {
            this->log_error(tr::f_("node not found to send message: {}", NodeIdTraits::stringify(id)));
            return false;
        }

        ptr->enqueue_packet(id, priority, std::move(data));
        return true;
    }

    bool enqueue_packet (node_id id, int priority, char const * data, std::size_t len)
    {
        auto ptr = locate_writer(id);

        if (ptr == nullptr) {
            this->log_error(tr::f_("node not found to send message: {}", NodeIdTraits::stringify(id)));
            return false;
        }

        ptr->enqueue_packet(id, priority, data, len);
        return true;
    }

    void process_route_received (node_id id, bool is_response, route_info const & rinfo)
    {
        if (is_response) {
            auto responder_id = node_id_traits::make(rinfo.responder_id.first, rinfo.responder_id.second);

            // Initiator node received response - add route to the routing table.
            if (rinfo.initiator_id == _id_pair) {
                auto hops = pfs::numeric_cast<unsigned int>(rinfo.route.size());
                LOGD("***", "Route complete: responder ID: {}, hops={}"
                    , node_id_traits::stringify(responder_id), hops);

                _rtab.add_route(responder_id, id, hops);
                return;
            }

            // Add route to responder to routing table and forward response.
            if (_is_gateway) {
                PFS__TERMINATE(!rinfo.route.empty(), "Fix meshnet::node_pool algorithm");

                // Find this gateway
                auto opt_index = rinfo.gateway_index(_id_pair);

                PFS__TERMINATE(opt_index, "Fix meshnet::node_pool algorithm");

                auto index = *opt_index;

                // This gateway is the first one for responder
                if (rinfo.route.size() == 1 || index == rinfo.route.size() - 1) {
                    _rtab.add_route(responder_id, _id, 0);
                } else {
                    PFS__TERMINATE(rinfo.route.size() > index, "Fix meshnet::node_pool algorithm");

                    auto hops = static_cast<unsigned int>(rinfo.route.size() - index - 1);

                    _rtab.add_route(responder_id, id, hops);
                }

                // Forward
                std::vector<char> msg = _rtab.build_route_response(rinfo, false);

                // This gateway is the first one for request initiator
                if (index == 0) {
                    auto addressee_id = node_id_traits::make(rinfo.initiator_id.first, rinfo.initiator_id.second);
                    enqueue_packet(addressee_id, 0, std::move(msg));
                } else {
                    auto addressee_id = node_id_traits::make(rinfo.route[index - 1].first
                        , rinfo.route[index - 1].second);
                    enqueue_packet(addressee_id, 0, std::move(msg));
                }
            }
        } else { // Request
            auto initiator_id = node_id_traits::make(rinfo.initiator_id.first, rinfo.initiator_id.second);

            // Add record to the routing table about the route to the initiator
            if (_is_gateway) {
                if (rinfo.route.empty()) {
                    // First gateway for the initiator (direct access).
                    // For direct access no matter the gateway.
                    _rtab.add_route(initiator_id, _id, 0);
                } else {
                    _rtab.add_route(initiator_id, id, rinfo.route.size());
                }
            } else {
                PFS__TERMINATE(!rinfo.route.empty(), "Fix meshnet::node_pool algorithm");
                _rtab.add_route(initiator_id, id, rinfo.route.size());
            }

            // Initiate response and transmit it by the reverse route
            std::vector<char> msg = _rtab.build_route_response(rinfo, true);
            enqueue_packet(id, 0, std::move(msg));

            // Forward request (broadcast) to nearest nodes
            if (_is_gateway) {
                std::vector<char> msg = _rtab.update_route_request(rinfo);
                auto idx = find_node_index(id);

                PFS__TERMINATE(idx != INVALID_NODE_INDEX, "Fix meshnet::node_pool algorithm");

                for (node_index_t i = 0; i < _nodes.size(); i++) {
                    // Exclude subnet
                    if (i + 1 == idx)
                        continue;

                    _nodes[i]->enqueue_broadcast_packet(0, msg.data(), msg.size());
                }
            }
        }
    }
public:
    node_id id () const noexcept
    {
        return _id;
    }

    bool behind_nat () const noexcept
    {
        return _behind_nat;
    }

    bool is_gateway () const noexcept
    {
        return _is_gateway;
    }

    /**
     * Adds new node to the pool with specified listeners.
     */
    template <typename Node, typename ListenerIt>
    node_index_t add_node (ListenerIt first, ListenerIt last, error * perr = nullptr)
    {
        static_assert(std::is_same<typename Node::callback_suite, node_callbacks<node_id>>::value
            , "Incompatible callback for node");

        auto node = Node::template make_interface(_id, _behind_nat, _is_gateway, _node_callbacks);
        error err;

        for (ListenerIt pos = first; pos != last; ++pos) {
            node->add_listener(*pos, & err);

            if (err)
                pfs::throw_or(perr, std::move(err));
        }

        _nodes.push_back(std::move(node));
        return static_cast<node_index_t>(_nodes.size());
    }

    /**
     * Adds new channel to the node with specified listeners.
     */
    template <typename Node>
    node_index_t add_node (std::vector<socket4_addr> const & listener_saddrs, error * perr = nullptr)
    {
        return add_node<Node>(listener_saddrs.begin(), listener_saddrs.end(), perr);
    }

    /**
     * Adds new node to the node pool with specified listeners.
     */
    template <typename Node>
    node_index_t add_node (std::initializer_list<socket4_addr> const & listener_saddrs, error * perr = nullptr)
    {
        return add_node<Node>(listener_saddrs.begin(), listener_saddrs.end(), perr);
    }

    void listen (int backlog = 50)
    {
        for (auto & x: _nodes)
            x->listen(backlog);
    }

    void listen (node_index_t index, int backlog)
    {
        auto ptr = locate_node(index);

        if (ptr == nullptr)
            return;

        ptr->listen(backlog);
    }

    bool connect_host (node_index_t index, netty::socket4_addr remote_saddr)
    {
        auto ptr = locate_node(index);

        if (ptr == nullptr)
            return false;

        return ptr->connect_host(remote_saddr);
    }

    bool connect_host (node_index_t index, netty::socket4_addr remote_saddr, netty::inet4_addr local_addr)
    {
        auto ptr = locate_node(index);

        if (ptr == nullptr)
            return false;

        return ptr->connect_host(remote_saddr, local_addr);
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
        auto ptr = locate_writer(id);

        if (ptr == nullptr) {
            this->log_error(tr::f_("node not found to send message: {}", NodeIdTraits::stringify(id)));
            return false;
        }

        ptr->enqueue(id, priority, force_checksum, data, len);
        return true;
    }

    bool enqueue (node_id id, int priority, bool force_checksum, std::vector<char> && data)
    {
        auto ptr = locate_writer(id);

        if (ptr == nullptr) {
            this->log_error(tr::f_("node not found to send message: {}", NodeIdTraits::stringify(id)));
            return false;
        }

        ptr->enqueue(id, priority, force_checksum, std::move(data));
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

    bool enqueue (node_id id, int priority, std::vector<char> && data)
    {
        return enqueue(id, priority, false, std::move(data));
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
