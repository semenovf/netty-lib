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
#include "../../trace.hpp"
#include "functional_callbacks.hpp"
#include "node_index.hpp"
#include "node_interface.hpp"
#include "route.hpp"
#include "route_info.hpp"
#include <pfs/assert.hpp>
#include <pfs/countdown_timer.hpp>
#include <pfs/i18n.hpp>
#include <pfs/numeric_cast.hpp>
#include <atomic>
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
    , typename AliveProcessor
    , typename CallbackSuite>
class node_pool
{
    using node_interface_type = node_interface<NodeIdTraits>;
    using node_interface_ptr = std::unique_ptr<node_interface_type>;
    using routing_table_type = RoutingTable;
    using alive_processor_type = AliveProcessor;

public:
    using node_id_traits = NodeIdTraits;
    using node_id_rep = typename NodeIdTraits::rep_type;
    using node_id = typename NodeIdTraits::type;
    using callback_suite = CallbackSuite;

    struct options
    {
        node_id id;
        std::string name;
        bool is_gateway {false};
    };

private:
    node_id_rep _id_rep; // Node ID representation

    // User friendly node identifier, default value is stringified representation of node identifier
    std::string _name;

    // True if the node is a gateway
    bool _is_gateway {false};

    // There will rarely be more than dozens nodes, so vector is a optimal choice
    std::vector<node_interface_ptr> _nodes;

    // Routing table
    routing_table_type _rtab;

    alive_processor_type _aproc;

    std::shared_ptr<callback_suite> _callbacks;

    std::shared_ptr<node_callbacks> _node_callbacks;

    std::atomic_bool _interrupted {false};

public:
    node_pool (options && opts, std::shared_ptr<callback_suite> callbacks)
        : _id_rep(node_id_traits::cast(opts.id))
        , _is_gateway(opts.is_gateway)
        , _aproc(node_id_traits::cast(opts.id))
        , _callbacks(callbacks)
    {
        if (opts.name.empty())
            _name = node_id_traits::to_string(opts.id);
        else
            _name = std::move(opts.name);

        _node_callbacks = std::make_shared<node_callbacks>();

        _node_callbacks->on_error = [this] (std::string const & msg) {
            _callbacks->on_error(msg);
        };

        _node_callbacks->on_channel_established = [this] (node_id_rep const & id_rep, node_index_t index, bool is_gateway) {
            // Start routes discovery and initiate alive exchange if channel established with gateway
            if (is_gateway) {
                _rtab.append_gateway(id_rep);

                std::vector<char> msg = _rtab.serialize_request(_id_rep);
                this->enqueue_packet(id_rep, 0, std::move(msg));
            }

            // Add direct route
            _rtab.add_sibling(id_rep);

            // Add sibling node as alive
            _aproc.add_sibling(id_rep);

            _callbacks->on_channel_established(id_rep, is_gateway);
        };

        _node_callbacks->on_channel_destroyed = [this] (node_id_rep const & id_rep, node_index_t index) {
            _rtab.remove_sibling(id_rep);

            if (_is_gateway) {
                // Broadcast unreachable packet
                std::vector<char> msg = _aproc.serialize_unreachable(id_rep);
                retransmit_packet(index, std::move(msg));
            }

            _aproc.expire(id_rep);

            _callbacks->on_channel_destroyed(id_rep);
        };

        _node_callbacks->on_bytes_written = [this] (node_id_rep const & id_rep, std::uint64_t n) {
            _callbacks->on_bytes_written(id_rep, n);
        };

        _node_callbacks->on_alive_received = [this] (node_id_rep const & id_rep, node_index_t index, alive_info const & ainfo) {
            process_alive_received(id_rep, index, ainfo);
        };

        _node_callbacks->on_unreachable_received = [this] (node_id_rep const & id_rep, node_index_t index, unreachable_info const & uinfo) {
            process_unreachable_received(id_rep, index, uinfo);
        };

        _node_callbacks->on_route_received = [this] (node_id_rep const & id_rep, node_index_t index, bool is_response, route_info const & rinfo) {
            process_route_received(id_rep, index, is_response, rinfo);
        };

        _node_callbacks->on_domestic_message_received = [this] (node_id_rep const & id_rep, int priority, std::vector<char> && bytes) {
            _callbacks->on_message_received(id_rep, priority, std::move(bytes));
        };

        _node_callbacks->on_global_message_received = [this] (node_id_rep const & /*id_rep*/, int priority
                , node_id_rep const & sender_id, node_id_rep const & receiver_id, std::vector<char> && bytes) {

            PFS__TERMINATE(_id_rep == receiver_id, "Fix meshnet::node_pool algorithm");
            _callbacks->on_message_received(sender_id, priority, std::move(bytes));
        };

        _node_callbacks->forward_global_message = [this] (int priority
                , node_id_rep const & /*sender_id*/, node_id_rep const & receiver_id, std::vector<char> && packet) {

            PFS__TERMINATE(_id_rep != receiver_id && _is_gateway, "Fix meshnet::node_pool algorithm");
            enqueue_packet(receiver_id, priority, std::move(packet));
        };

        _aproc.on_alive([this] (node_id_rep const & id_rep) {
            _callbacks->on_node_alive(id_rep);
        });

        _aproc.on_expired([this] (node_id_rep const & id_rep) {
            _callbacks->on_node_expired(id_rep);
        });
    }

    node_pool (node_pool const &) = delete;
    node_pool (node_pool &&) = delete;
    node_pool & operator = (node_pool const &) = delete;
    node_pool & operator = (node_pool &&) = delete;

    ~node_pool () = default;

private:
    node_interface_type * locate_node (node_id_rep const & id_rep)
    {
        if (_nodes[0]->id_rep() == id_rep)
            return & *_nodes[0];

        for (int i = 1; i < _nodes.size(); i++) {
            if (_nodes[i]->id_rep() == id_rep) {
                return & *_nodes[i];
            }
        }

        return nullptr;
    }

    node_interface_type * locate_writer (node_id_rep const & id_rep)
    {
        if (_nodes.empty())
            return nullptr;

        auto gwid_opt = _rtab.gateway_for(id_rep);

        // No route or it is unreachable
        if (!gwid_opt)
            return nullptr;

        if (_nodes[0]->has_writer(*gwid_opt))
            return & *_nodes[0];

        for (int i = 1; i < _nodes.size(); i++) {
            if (_nodes[i]->has_writer(*gwid_opt)) {
                return & *_nodes[i];
            }
        }

        return nullptr;
    }

    node_interface_type * locate_node (node_index_t index)
    {
        if (index < 1 && index > _nodes.size()) {
            _callbacks->on_error(tr::f_("node index is out of bounds: {}", index));
            return nullptr;
        }

        return & *_nodes[index - 1];
    }

    bool enqueue_packet (node_id_rep const & id, int priority, std::vector<char> && data)
    {
        auto ptr = locate_writer(id);

        if (ptr == nullptr) {
            _callbacks->on_error(tr::f_("node not found to send packet: {}", _name));
            return false;
        }

        ptr->enqueue_packet(id, priority, std::move(data));
        return true;
    }

    bool enqueue_packet (node_id_rep const & id, int priority, char const * data, std::size_t len)
    {
        auto ptr = locate_writer(id);

        if (ptr == nullptr) {
            _callbacks->on_error(tr::f_("node not found to send packet: {}", _name));
            return false;
        }

        ptr->enqueue_packet(id, priority, data, len);
        return true;
    }

    void process_alive_received (node_id_rep const & /*id*/, node_index_t idx, alive_info const & ainfo)
    {
        auto initiator_id = ainfo.id;
        auto updated = _aproc.update_if(initiator_id);

        if (updated && _is_gateway) {
            // Forward packet to nearest nodes
            std::vector<char> msg = _aproc.serialize_alive(ainfo);
            retransmit_packet(idx, std::move(msg));
        }
    }

    void process_unreachable_received (node_id_rep const & id, node_index_t idx, unreachable_info const & uinfo)
    {
        // FIXME
        // auto addressee_id = uinfo.id;
        //
        // // Disable route to `uinfo.id` through `id`.
        // _rtab.enabled_route(addressee_id, id, false);
        //
        // if (_is_gateway) {
        //     // Forward packet to nearest nodes
        //     std::vector<char> msg = _aproc.serialize_unreachable(uinfo);
        //     retransmit_packet(idx, std::move(msg));
        // } else {
        //     _aproc.expire(addressee_id);
        // }
    }

    void process_route_received (node_id_rep const & id, node_index_t idx, bool is_response
        , route_info const & rinfo)
    {
        if (is_response) {
            auto responder_id = rinfo.responder_id;

            // Initiator node received response - add route to the routing table.
            if (rinfo.initiator_id == _id_rep) {
                auto hops = pfs::numeric_cast<unsigned int>(rinfo.route.size());

                if (hops == 0) {
                    _rtab.add_sibling(responder_id);
                } else {
                    _rtab.add_route(responder_id, rinfo, route_order_enum::reverse);
                }

                NETTY__TRACE("[node_pool]", "routes dump:\n{}", dump_routes(4));

                return;
            }

            // Add route to responder to routing table and forward response.
            if (_is_gateway) {
                PFS__TERMINATE(!rinfo.route.empty(), "Fix meshnet::node_pool algorithm");

                // Find this gateway
                auto opt_index = rinfo.gateway_index(_id_rep);

                PFS__TERMINATE(opt_index, "Fix meshnet::node_pool algorithm");

                auto index = *opt_index;

                // This gateway is the first one for responder
                if (rinfo.route.size() == 1 || index == rinfo.route.size() - 1) {
                    _rtab.add_sibling(responder_id);
                } else {
                    PFS__TERMINATE(rinfo.route.size() > index, "Fix meshnet::node_pool algorithm");

                    _rtab.add_route(responder_id, rinfo, route_order_enum::reverse);

                    // Receiving a route response is an indication that the responder is active.
                    _aproc.update_if(responder_id);
                }

                NETTY__TRACE("[node_pool]", "routes dump:\n{}", dump_routes(4));

                // Forward
                std::vector<char> msg = _rtab.serialize_response(rinfo);

                // This gateway is the first one for request initiator
                if (index == 0) {
                    auto addressee_id = rinfo.initiator_id;
                    enqueue_packet(addressee_id, 0, std::move(msg));
                } else {
                    auto addressee_id = rinfo.route[index - 1];
                    enqueue_packet(addressee_id, 0, std::move(msg));
                }
            }
        } else { // Request
            auto initiator_id = rinfo.initiator_id;

            // Add record to the routing table about the route to the initiator
            if (_is_gateway) {
                if (rinfo.route.empty()) {
                    // First gateway for the initiator (direct access).
                    _rtab.add_sibling(initiator_id);
                } else {
                    _rtab.add_route(initiator_id, rinfo, route_order_enum::direct);
                }
            } else {
                PFS__TERMINATE(!rinfo.route.empty(), "Fix meshnet::node_pool algorithm");
                _rtab.add_route(initiator_id, rinfo, route_order_enum::direct);
            }

            NETTY__TRACE("[node_pool]", "routes dump:\n{}", dump_routes(4));

            // Initiate response and transmit it by the reverse route
            std::vector<char> msg = _rtab.serialize_response(id, rinfo);
            enqueue_packet(id, 0, std::move(msg));

            // Forward request (broadcast) to nearest nodes
            if (_is_gateway) {
                std::vector<char> msg = _rtab.serialize_request(_id_rep, rinfo);
                retransmit_packet(idx, std::move(msg));
            }
        }
    }

    /**
     * Forward packet (broadcast) to nearest nodes excluding nodes connected to the one with
     * index @a excluded_index.
     *
     * @param excluded_index Node index from which packet received.
     * @param data Serialized packet.
     */
    void retransmit_packet (node_index_t excluded_index, std::vector<char> && data)
    {
        // auto idx = find_node_index(sender_id);

        PFS__TERMINATE(excluded_index != INVALID_NODE_INDEX, "Fix meshnet::node_pool algorithm");

        for (node_index_t i = 0; i < _nodes.size(); i++) {
            // Exclude node
            if (i + 1 == excluded_index)
                continue;

            _nodes[i]->enqueue_broadcast_packet(0, data.data(), data.size());
        }
    }

    /**
     * Check _rtab.gateway_count() > 0 before call this method.
     */
    void broadcast_alive ()
    {
        auto msg = _aproc.serialize_alive();

        _rtab.foreach_gateway([this, & msg] (node_id_rep const & gwid) {
            if (_aproc.is_alive(gwid))
                enqueue_packet(gwid, 0, msg.data(), msg.size());
        });
    }

#if NETTY__TRACE_ENABLED
    std::string dump_routes (int indent = 0)
    {
        std::string result;

        _rtab.foreach_route([this, & result, indent] (node_id_rep const & dest, node_id_rep const & gwid, unsigned int hops) {
            auto node_ptr = locate_node(dest);

            PFS__ASSERT(node_ptr != nullptr, "");

            if (hops == 0) {
                result += fmt::format("{:{}} <this node> -> {}: direct\n", "", indent, node_ptr->name());
            } else {
                auto gw_ptr = locate_node(gwid);

                PFS__ASSERT(gw_ptr != nullptr, "");

                result += fmt::format("{:{}} <this node> -> {} -> ... -> {}: {} hops\n", "", indent
                    , gw_ptr->name(), node_ptr->name(), hops);
            }
        });

        return result;
    }
#endif

public:
    node_id_rep const & id_rep () const noexcept
    {
        return _id_rep;
    }

    node_id id () const noexcept
    {
        return node_id_traits::cast(_id_rep);
    }

    bool is_gateway () const noexcept
    {
        return _is_gateway;
    }

    void interrupt ()
    {
        _interrupted = true;
    }

    /**
     * Adds new node to the pool with specified listeners.
     */
    template <typename Node, typename ListenerIt>
    node_index_t add_node (ListenerIt first, ListenerIt last, error * perr = nullptr)
    {
        static_assert(std::is_same<typename Node::callback_suite, node_callbacks>::value
            , "Incompatible callback for node");

        typename Node::options opts;
        opts.id = node_id_traits::cast(_id_rep);
        opts.name = _name;
        opts.is_gateway = _is_gateway;

        auto node = Node::template make_interface(std::move(opts), _node_callbacks);
        error err;

        for (ListenerIt pos = first; pos != last; ++pos) {
            node->add_listener(*pos, & err);

            if (err)
                pfs::throw_or(perr, std::move(err));
        }

        _nodes.push_back(std::move(node));

        node_index_t index = static_cast<node_index_t>(_nodes.size());
        _nodes.back()->set_index(index);
        return index;
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

    bool connect_host (node_index_t index, netty::socket4_addr remote_saddr, bool behind_nat = false)
    {
        auto ptr = locate_node(index);

        if (ptr == nullptr)
            return false;

        return ptr->connect_host(remote_saddr, behind_nat);
    }

    bool connect_host (node_index_t index, netty::socket4_addr remote_saddr, netty::inet4_addr local_addr
        , bool behind_nat = false)
    {
        auto ptr = locate_node(index);

        if (ptr == nullptr)
            return false;

        return ptr->connect_host(remote_saddr, local_addr, behind_nat);
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
    bool enqueue (node_id_rep const & id, int priority, bool force_checksum, char const * data, std::size_t len)
    {
        auto ptr = locate_writer(id);

        if (ptr == nullptr) {
            _callbacks->on_error(tr::f_("node not found to send message: {}", NodeIdTraits::stringify(id)));
            return false;
        }

        ptr->enqueue(id, priority, force_checksum, data, len);
        return true;
    }

    bool enqueue (node_id_rep const & id, int priority, bool force_checksum, std::vector<char> && data)
    {
        auto ptr = locate_writer(id);

        if (ptr == nullptr) {
            _callbacks->on_error(tr::f_("node not found to send message: {}", NodeIdTraits::stringify(id)));
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
    bool enqueue (node_id_rep const & id, int priority, char const * data, std::size_t len)
    {
        return enqueue(id, priority, false, data, len);
    }

    bool enqueue (node_id_rep const & id, int priority, std::vector<char> && data)
    {
        return enqueue(id, priority, false, std::move(data));
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step ()
    {
        unsigned int result = 0;

        for (auto & x: _nodes)
            result += x->step();

        // The presence of a gateway is an indication that `alive` packet needs to be sent.
        if (_rtab.gateway_count() > 0) {
            // Broadcast `alive` packet
            if (_aproc.interval_exceeded()) {
                _aproc.update_notification_time();
                broadcast_alive();
            }

            // Check alive expiration
            _aproc.check_expiration();
        }

        return result;
    }

    void run (std::chrono::milliseconds loop_interval = std::chrono::milliseconds{10})
    {
        while (!_interrupted) {
            pfs::countdown_timer<std::milli> countdown_timer {loop_interval};
            auto n = step();

            if (n == 0)
                std::this_thread::sleep_for(countdown_timer.remain());
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
