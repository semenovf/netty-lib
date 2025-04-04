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

    callback_suite _callbacks;

    std::shared_ptr<node_callbacks> _node_callbacks;

    std::atomic_bool _interrupted {false};

public:
    node_pool (options && opts, callback_suite && callbacks)
        : _id_rep(node_id_traits::cast(opts.id))
        , _is_gateway(opts.is_gateway)
        , _aproc(node_id_traits::cast(opts.id))
        , _callbacks(std::move(callbacks))
    {
        if (opts.name.empty())
            _name = node_id_traits::to_string(opts.id);
        else
            _name = std::move(opts.name);

        _node_callbacks = std::make_shared<node_callbacks>();

        _node_callbacks->on_error = [this] (std::string const & msg) {
            _callbacks.on_error(msg);
        };

        _node_callbacks->on_channel_established = [this] (node_id_rep id_rep
                , node_index_t index, bool is_gateway) {
            // Add direct route
            auto route_added = _rtab.add_sibling(id_rep);

            // Add sibling node as alive
            _aproc.add_sibling(id_rep);

            // Start routes discovery and initiate alive exchange if channel established with gateway
            if (is_gateway) {
                _rtab.add_gateway(id_rep);

                std::vector<char> msg = _rtab.serialize_request(_id_rep);
                this->enqueue_packet(id_rep, 0, std::move(msg));
            }

            _callbacks.on_channel_established(id_rep, is_gateway);

            if (route_added)
                _callbacks.on_route_ready(id_rep, 0);
        };

        _node_callbacks->on_channel_destroyed = [this] (node_id_rep id_rep, node_index_t /*index*/) {
            _rtab.remove_sibling(id_rep);

            // FIXME
            // if (_is_gateway) {
            //     // Broadcast unreachable packet
            //     std::vector<char> msg = _aproc.serialize_unreachable(id_rep);
            //     forward_packet(std::move(msg));
            // }

            _aproc.expire(id_rep);

            _callbacks.on_channel_destroyed(id_rep);
        };

        _node_callbacks->on_bytes_written = [this] (node_id_rep id_rep, std::uint64_t n) {
            _callbacks.on_bytes_written(id_rep, n);
        };

        _node_callbacks->on_alive_received = [this] (node_id_rep id_rep, node_index_t index
                , alive_info const & ainfo) {
            process_alive_received(id_rep, index, ainfo);
        };

        _node_callbacks->on_unreachable_received = [this] (node_id_rep id_rep
                , node_index_t index, unreachable_info const & uinfo) {
            process_unreachable_received(id_rep, index, uinfo);
        };

        _node_callbacks->on_route_received = [this] (node_id_rep id_rep, node_index_t index
                , bool is_response, route_info const & rinfo) {
            process_route_received(id_rep, index, is_response, rinfo);
        };

        _node_callbacks->on_domestic_message_received = [this] (node_id_rep id_rep
                , int priority, std::vector<char> && bytes) {
            _callbacks.on_message_received(id_rep, priority, std::move(bytes));
        };

        _node_callbacks->on_global_message_received = [this] (node_id_rep /*id_rep*/
                , int priority, node_id_rep sender_id, node_id_rep receiver_id
                , std::vector<char> && bytes) {
            PFS__TERMINATE(_id_rep == receiver_id, "Fix meshnet::node_pool algorithm");
            _callbacks.on_message_received(sender_id, priority, std::move(bytes));
        };

        _node_callbacks->forward_global_message = [this] (int priority
                , node_id_rep /*sender_id*/, node_id_rep receiver_id
                , std::vector<char> && packet) {
            PFS__TERMINATE(_id_rep != receiver_id && _is_gateway, "Fix meshnet::node_pool algorithm");
            enqueue_packet(receiver_id, priority, std::move(packet));
        };

        _aproc.on_alive([this] (node_id_rep id_rep) {
            _callbacks.on_node_alive(id_rep);
        });

        _aproc.on_expired([this] (node_id_rep id_rep) {
            _callbacks.on_node_expired(id_rep);
        });
    }

    node_pool (node_pool const &) = delete;
    node_pool (node_pool &&) = delete;
    node_pool & operator = (node_pool const &) = delete;
    node_pool & operator = (node_pool &&) = delete;

    ~node_pool ()
    {
        clear_all_channels();
    }

private:
    node_interface_type * locate_node (node_id_rep id_rep)
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

    node_interface_type * locate_writer (node_id_rep id_rep)
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
            _callbacks.on_error(tr::f_("node index is out of bounds: {}", index));
            return nullptr;
        }

        return & *_nodes[index - 1];
    }

    bool enqueue_packet (node_id_rep id, int priority, std::vector<char> && data)
    {
        auto ptr = locate_writer(id);

        if (ptr == nullptr) {
            _callbacks.on_error(tr::f_("node not found to send packet: {}", _name));
            return false;
        }

        ptr->enqueue_packet(id, priority, std::move(data));
        return true;
    }

    bool enqueue_packet (node_id_rep id, int priority, char const * data, std::size_t len)
    {
        auto ptr = locate_writer(id);

        if (ptr == nullptr) {
            _callbacks.on_error(tr::f_("node not found to send packet: {}", _name));
            return false;
        }

        ptr->enqueue_packet(id, priority, data, len);
        return true;
    }

    void process_alive_received (node_id_rep id_rep, node_index_t idx, alive_info const & ainfo)
    {
        auto initiator_id = ainfo.id;
        auto updated = _aproc.update_if(initiator_id);

        if (updated && _is_gateway) {
            // Forward packet to nearest nodes
            std::vector<char> msg = _aproc.serialize_alive(ainfo);
            forward_packet(id_rep, std::move(msg));
        }
    }

    void process_unreachable_received (node_id_rep id, node_index_t idx, unreachable_info const & uinfo)
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
        //     forward_packet(idx, std::move(msg));
        // } else {
        //     _aproc.expire(addressee_id);
        // }
    }

    void process_route_received (node_id_rep id_rep, node_index_t idx, bool is_response
        , route_info const & rinfo)
    {
        bool new_route_added = false;
        std::uint16_t hops = 0;
        node_id_rep dest_id_rep {};

        if (is_response) {
            dest_id_rep = rinfo.responder_id;
            hops = pfs::numeric_cast<std::uint16_t>(rinfo.route.size());

            // Initiator node received response - add route to the routing table.
            if (rinfo.initiator_id == _id_rep) {
                if (hops == 0)
                    new_route_added = _rtab.add_sibling(dest_id_rep);
                else
                    new_route_added = _rtab.add_route(dest_id_rep, rinfo, route_order_enum::direct);
            } else if (_is_gateway) {
                // Add route to responder to routing table and forward response.

                PFS__TERMINATE(hops > 0, "Fix meshnet::node_pool algorithm");

                // Find this gateway
                auto opt_index = rinfo.gateway_index(_id_rep);

                PFS__TERMINATE(opt_index, "Fix meshnet::node_pool algorithm");

                auto index = *opt_index;

                // This gateway is the first one for responder
                if (index == rinfo.route.size() - 1) {
                    // NOTE. There are no known cases when this will happen, as the sibling node has
                    // already been added previously. But let it be for insurance purposes.
                    new_route_added = _rtab.add_sibling(dest_id_rep);
                } else {
                    new_route_added = _rtab.add_subroute(dest_id_rep, _id_rep, rinfo, route_order_enum::direct);

                    // Receiving a route response is an indication that the responder is active.
                    _aproc.update_if(dest_id_rep);
                }

                // Serialize response and send to previous gateway (if index > 0)
                // or to the initiator node
                std::vector<char> msg = _rtab.serialize_response(rinfo);

                if (index > 0) {
                    auto addressee_id = rinfo.route[index - 1];
                    enqueue_packet(addressee_id, 0, std::move(msg));
                } else {
                    auto addressee_id = rinfo.initiator_id;
                    enqueue_packet(addressee_id, 0, std::move(msg));
                }
            } else {
                PFS__TERMINATE(false, "Fix meshnet::node_pool algorithm");
            }
        } else { // Request
            dest_id_rep = rinfo.initiator_id;
            hops = pfs::numeric_cast<std::uint16_t>(rinfo.route.size());

            // Add record to the routing table about the route to the initiator
            if (_is_gateway) {
                if (hops == 0)
                    new_route_added = _rtab.add_sibling(dest_id_rep);
                else
                    new_route_added = _rtab.add_route(dest_id_rep, rinfo, route_order_enum::reverse);
            } else {
                PFS__TERMINATE(hops > 0, "Fix meshnet::node_pool algorithm");
                new_route_added = _rtab.add_route(dest_id_rep, rinfo, route_order_enum::reverse);
            }

            // Initiate response and transmit it by the reverse route
            std::vector<char> msg = _rtab.serialize_response(_id_rep, rinfo);
            enqueue_packet(id_rep, 0, std::move(msg));

            // Forward request to nearest nodes if this gateway is not present in the received route
            // to prevent cycling.
            if (_is_gateway) {
                auto opt_index = rinfo.gateway_index(_id_rep);

                if (!opt_index) {
                    std::vector<char> msg = _rtab.serialize_request(_id_rep, rinfo);
                    forward_packet(id_rep, std::move(msg));
                }
            }
        }

        if (new_route_added)
            _callbacks.on_route_ready(dest_id_rep, hops);
    }

    /**
     * Forward packet to nearest nodes excluding node identified by @a sender_id_rep.
     *
     * @param data Serialized packet.
     */
    void forward_packet (node_id_rep sender_id_rep, std::vector<char> && data)
    {
        for (node_index_t i = 0; i < _nodes.size(); i++)
            _nodes[i]->enqueue_forward_packet(sender_id_rep, 0, data.data(), data.size());
    }

    /**
     * Check _rtab.gateway_count() > 0 before call this method.
     */
    void broadcast_alive ()
    {
        // FIXME UNCOMMENT
        // auto msg = _aproc.serialize_alive();
        //
        // _rtab.foreach_gateway([this, & msg] (node_id_rep gwid) {
        //     if (_aproc.is_alive(gwid))
        //         enqueue_packet(gwid, 0, msg.data(), msg.size());
        // });
    }

public:
    node_id_rep id_rep () const noexcept
    {
        return _id_rep;
    }

    node_id id () const noexcept
    {
        return node_id_traits::cast(_id_rep);
    }

    std::string name () const noexcept
    {
        return _name;
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
     * Equeues message for delivery to specified node ID @a id_rep.
     *
     * @param id_rep Receiver ID.
     * @param priority Message priority.
     * @param force_checksum Calculate checksum before sending.
     * @param data Message content.
     * @param len Length of the message content.
     *
     * @return @c true if message route found for @a id_rep.
     */
    bool enqueue (node_id_rep id_rep, int priority, bool force_checksum, char const * data
        , std::size_t len)
    {
        auto ptr = locate_writer(id_rep);

        if (ptr == nullptr) {
            _callbacks.on_error(tr::f_("node not found to send message: {}"
                , node_id_traits::to_string(id_rep)));
            return false;
        }

        ptr->enqueue(id_rep, priority, force_checksum, data, len);
        return true;
    }

    bool enqueue (node_id_rep id_rep, int priority, bool force_checksum
        , std::vector<char> && data)
    {
        auto ptr = locate_writer(id_rep);

        if (ptr == nullptr) {
            _callbacks.on_error(tr::f_("node not found to send message: {}"
                , node_id_traits::to_string(id_rep)));
            return false;
        }

        ptr->enqueue(id_rep, priority, force_checksum, std::move(data));
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
    bool enqueue (node_id_rep id, int priority, char const * data, std::size_t len)
    {
        return enqueue(id, priority, false, data, len);
    }

    bool enqueue (node_id_rep id, int priority, std::vector<char> && data)
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
        _interrupted.store(false);

        while (!_interrupted) {
            pfs::countdown_timer<std::milli> countdown_timer {loop_interval};
            auto n = step();

            if (n == 0)
                std::this_thread::sleep_for(countdown_timer.remain());
        }
    }

    /**
     * Close all channels for all nodes
     */
    void clear_all_channels ()
    {
        if (!_nodes.empty()) {
            for (auto & x: _nodes)
                x->clear_channels();
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
