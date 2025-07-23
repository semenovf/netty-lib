////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.24 Initial version.
//      2025.06.30 Added method `set_frame_size()`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../callback.hpp"
#include "../../trace.hpp"
#include "node_index.hpp"
#include "node_interface.hpp"
#include "route_info.hpp"
#include "tag.hpp"
#include <pfs/assert.hpp>
#include <pfs/countdown_timer.hpp>
#include <pfs/i18n.hpp>
#include <pfs/log.hpp>
#include <pfs/numeric_cast.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeId
    , typename RoutingTable
    , typename AliveController
    , typename RecursiveWriterMutex>
class node_pool
{
    using node_interface_type = node_interface<NodeId>;
    using node_interface_ptr = std::unique_ptr<node_interface_type>;
    using routing_table_type = RoutingTable;
    using alive_controller_type = AliveController;
    using writer_mutex_type = RecursiveWriterMutex;

public:
    using node_id = NodeId;
    using address_type = node_id;
    using gateway_chain_type = typename routing_table_type::gateway_chain_type;

private:
    node_id _id;

    // True if the node is a gateway
    bool _is_gateway {false};

    // There will rarely be more than dozens nodes, so vector is a optimal choice
    std::vector<node_interface_ptr> _nodes;

    routing_table_type _rtab;
    alive_controller_type _alive_controller;
    std::atomic_bool _interrupted {false};

    // Writer mutex to protect sending
    writer_mutex_type _writer_mtx;

private:
    callback_t<void (std::string const &)> _on_error
        = [] (std::string const & errstr) { LOGE(TAG, "{}", errstr); };

    callback_t<void (node_id, bool)> _on_channel_established;
    callback_t<void (node_id)> _on_channel_destroyed;
    callback_t<void (node_id)> _on_expired;
    callback_t<void (node_id, socket4_addr)> _on_duplicate_id;
    callback_t<void (node_id, gateway_chain_type)> _on_route_ready;
    callback_t<void (node_id, int, std::vector<char>)> _on_data_received;

public:
    node_pool (node_id id, bool is_gateway = false)
        : _id(id)
        , _is_gateway(is_gateway)
        , _alive_controller(id)
    {
        _alive_controller.on_expired([this] (node_id id) {
            _rtab.remove_routes(id);

            if (_on_expired)
                _on_expired(id);
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

public: // Set callbacks
    /**
     * Sets error callback.
     *
     * @details Invoked when error occurred.
     *          Callback @a f signature must match:
     *          void (std::string const &)
     */
    template <typename F>
    node_pool & on_error (F && f)
    {
        _on_error = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when connection established with the remote node.
     *
     * @details Callback @a f signature must match:
     *          void (node_id id, bool is_gateway)
     */
    template <typename F>
    node_pool & on_channel_established (F && f)
    {
        _on_channel_established = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when the channel is destroyed with the remote node.
     *
     * @details Callback @a f signature must match:
     *          void (node_id id)
     */
    template <typename F>
    node_pool & on_channel_destroyed (F && f)
    {
        _on_channel_destroyed = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when a node with identical ID is detected.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, socket4_addr)
     */
    template <typename F>
    node_pool & on_duplicate_id (F && f)
    {
        _on_duplicate_id = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when node alive status changed.
     *
     * @details Callback @a f signature must match:
     *          void (node_id id)
     */
    template <typename F>
    node_pool & on_node_alive (F && f)
    {
        _alive_controller.on_alive(std::forward<F>(f));
        return *this;
    }

    /**
     * Notify when node alive status changed.
     *
     * @details Callback @a f signature must match void (node_id id).
     */
    template <typename F>
    node_pool & on_node_expired (F && f)
    {
        _on_expired = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when some route ready by request or response.
     *
     * @details Callback @a f signature must match:
     *          void (node_id id, gateway_chain_type gateway_chain)
     */
    template <typename F>
    node_pool & on_route_ready (F && f)
    {
        _on_route_ready = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when message received (domestic or global)
     *
     * @details Callback @a f signature must match:
     *          void (node_id sender, int priority, std::vector<char> bytes)
     */
    template <typename F>
    node_pool & on_data_received (F && f)
    {
        _on_data_received = std::forward<F>(f);
        return *this;
    }

private:
    node_interface_type * locate_node (node_id id)
    {
        if (_nodes[0]->id() == id)
            return & *_nodes[0];

        for (int i = 1; i < _nodes.size(); i++) {
            if (_nodes[i]->id() == id) {
                return & *_nodes[i];
            }
        }

        return nullptr;
    }

    node_interface_type * locate_writer (node_id id, node_id * gw_id = nullptr)
    {
        if (_nodes.empty())
            return nullptr;

        auto gw_id_opt = _rtab.gateway_for(id);

        // No route or it is unreachable
        if (!gw_id_opt)
            return nullptr;

        if (gw_id != nullptr)
            *gw_id = *gw_id_opt;

        if (_nodes[0]->has_writer(*gw_id_opt))
            return & *_nodes[0];

        for (int i = 1; i < _nodes.size(); i++) {
            if (_nodes[i]->has_writer(*gw_id_opt)) {
                return & *_nodes[i];
            }
        }

        return nullptr;
    }

    node_interface_type * locate_node (node_index_t index)
    {
        if (index < 1 || index > _nodes.size()) {
            _on_error(tr::f_("node index is out of bounds: {}", index));
            return nullptr;
        }

        return & *_nodes[index - 1];
    }

    bool enqueue_packet (node_id id, int priority, std::vector<char> && data)
    {
        auto ptr = locate_writer(id);

        if (ptr == nullptr) {
            _on_error(tr::f_("node not found to send packet: {}", to_string(id)));
            return false;
        }

        ptr->enqueue_packet(id, priority, std::move(data));
        return true;
    }

    bool enqueue_packet (node_id id, int priority, char const * data, std::size_t len)
    {
        auto ptr = locate_writer(id);

        if (ptr == nullptr) {
            _on_error(tr::f_("node not found to send packet: {}", to_string(id)));
            return false;
        }

        ptr->enqueue_packet(id, priority, data, len);
        return true;
    }

    void process_alive_received (node_id id, node_index_t /*idx*/, alive_info<node_id> const & ainfo)
    {
        auto initiator_id = ainfo.id;
        auto updated = _alive_controller.update_if(initiator_id);

        if (updated && _is_gateway) {
            // Forward packet to nearest nodes
            std::vector<char> msg = _alive_controller.serialize_alive(ainfo);
            forward_packet(id, std::move(msg));
        }
    }

    void process_unreachable_received (node_id id, node_index_t /*idx*/
        , unreachable_info<node_id> const & uinfo)
    {
        // Receiver cannot be reached through the specified gateway.
        // Disable all routes containing the specified gateway.
        _rtab.remove_route(uinfo.receiver_id, uinfo.gw_id);

        // Try alternative route.
        //
        // Since there is no information on which route the packet came, we will forward it to
        // the nearest gateway/node.
        node_id gw_id;
        auto ptr = locate_writer(uinfo.sender_id, & gw_id);

        if (ptr != nullptr) {
            // Need an example when such situation could happen.
            PFS__THROW_UNEXPECTED(gw_id != id, "Fix meshnet::node_pool algorithm");

            std::vector<char> msg = _alive_controller.serialize_unreachable(uinfo);
            ptr->enqueue_packet(gw_id, 0, std::move(msg));
            return;
        } else {
            // Sender is me and there are no alternative routes
            if (uinfo.sender_id == _id) {
                // Expire receiver
                _alive_controller.expire(uinfo.receiver_id);
            } else {
                // Stuck. Nothing can be done.
                _on_error(tr::f_("unable to notify sender about unreachable destination: {}->{}: no route"
                    , to_string(uinfo.sender_id)
                    , to_string(uinfo.receiver_id)));
            }
        }
    }

    void process_route_received (node_id id, node_index_t idx, bool is_response
        , route_info<node_id> const & rinfo)
    {
        bool new_route_added = false;
        std::size_t gw_chain_index {0};
        std::uint16_t hops = 0;
        node_id dest_id {};
        bool reverse_order = true;

        if (is_response) {
            dest_id = rinfo.responder_id;
            hops = pfs::numeric_cast<std::uint16_t>(rinfo.route.size());

            // Initiator node received response - add route to the routing table.
            if (rinfo.initiator_id == _id) {
                if (hops == 0) {
                    new_route_added = _rtab.add_sibling(dest_id);
                } else {
                    auto res = _rtab.add_route(dest_id, rinfo.route);
                    gw_chain_index = res.first;
                    new_route_added = res.second;
                }
            } else if (_is_gateway) {
                // Add route to responder to routing table and forward response.

                // If there is no loop
                if (_id != dest_id) {
                    PFS__THROW_UNEXPECTED(hops > 0, "Fix meshnet::node_pool algorithm");

                    // Find this gateway
                    auto opt_index = rinfo.gateway_index(_id);

                    PFS__THROW_UNEXPECTED(opt_index, "Fix meshnet::node_pool algorithm");

                    auto index = *opt_index;

                    // This gateway is the first one for responder
                    if (index == rinfo.route.size() - 1) {
                        // NOTE. There are no known cases when this will happen, as the sibling node has
                        // already been added previously. But let it be for insurance purposes.
                        new_route_added = _rtab.add_sibling(dest_id);
                    } else {
                        auto res = _rtab.add_subroute(dest_id, _id, rinfo.route);
                        gw_chain_index = res.first;
                        new_route_added = res.second;

                        // Receiving a route response is an indication that the responder is active.
                        _alive_controller.update_if(dest_id);
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
                }
            } else {
                PFS__THROW_UNEXPECTED(false, "Fix meshnet::node_pool algorithm");
            }
        } else { // Request
            dest_id = rinfo.initiator_id;
            hops = pfs::numeric_cast<std::uint16_t>(rinfo.route.size());

            // If there is no loop
            if (_id != dest_id) {
                // Add record to the routing table about the route to the initiator
                if (_is_gateway) {
                    if (hops == 0) {
                        new_route_added = _rtab.add_sibling(dest_id);
                    } else {
                        auto res = _rtab.add_route(dest_id, rinfo.route, reverse_order);
                        gw_chain_index = res.first;
                        new_route_added = res.second;
                    }
                } else {
                    PFS__THROW_UNEXPECTED(hops > 0, "Fix meshnet::node_pool algorithm");
                    auto res = _rtab.add_route(dest_id, rinfo.route, reverse_order);
                    gw_chain_index = res.first;
                    new_route_added = res.second;
                }

                // Initiate response and transmit it by the reverse route
                std::vector<char> msg = _rtab.serialize_response(_id, rinfo);
                enqueue_packet(id, 0, std::move(msg));

                // Forward request to nearest nodes if this gateway is not present in the received route
                // to prevent cycling.
                if (_is_gateway) {
                    auto opt_index = rinfo.gateway_index(_id);

                    if (!opt_index) {
                        std::vector<char> msg = _rtab.serialize_request(_id, rinfo);
                        forward_packet(id, std::move(msg));
                    }
                }
            }
        }

        if (new_route_added) {
            PFS__THROW_UNEXPECTED(_id != dest_id, "Fix meshnet::node_pool algorithm");

            if (_on_route_ready) {
                auto gw_chain = _rtab.gateway_chain_by_index(gw_chain_index);
                _on_route_ready(dest_id, std::move(gw_chain));
            }
        }
    }

    /**
     * Forward packet to nearest nodes excluding node identified by @a sender_id.
     *
     * @param data Serialized packet.
     */
    void forward_packet (node_id sender_id, std::vector<char> && data)
    {
        for (node_index_t i = 0; i < _nodes.size(); i++)
            _nodes[i]->enqueue_forward_packet(sender_id, 0, data.data(), data.size());
    }

    /**
     * Check _rtab.gateway_count() > 0 before call this method.
     */
    void broadcast_alive ()
    {
        auto msg = _alive_controller.serialize_alive();

        _rtab.foreach_gateway([this, & msg] (node_id gwid) {
            if (_alive_controller.is_alive(gwid))
                enqueue_packet(gwid, 0, msg.data(), msg.size());
        });
    }

public:
    node_id id () const noexcept
    {
        return _id;
    }

    bool is_gateway () const noexcept
    {
        return _is_gateway;
    }

    void interrupt ()
    {
        _interrupted = true;
    }

    bool interrupted () const noexcept
    {
        return _interrupted.load();
    }

    void clear_interrupted ()
    {
        _interrupted.store(false);
    }

    /**
     * Adds new node to the pool with specified listeners.
     */
    template <typename Node, typename ListenerIt>
    node_index_t add_node (ListenerIt first, ListenerIt last, error * perr = nullptr)
    {
        auto node = Node::template make_interface(_id, _is_gateway);
        error err;

        for (ListenerIt pos = first; pos != last; ++pos) {
            node->add_listener(*pos, & err);

            if (err)
                pfs::throw_or(perr, std::move(err));
        }

        //
        // Assign node callbacks
        //
        node->on_error(_on_error);

        node->on_channel_established([this] (node_id id, node_index_t, bool is_gateway) {
            _on_channel_established(id, is_gateway);

            // Add direct route
            auto route_added = _rtab.add_sibling(id);

            // Add sibling node as alive
            _alive_controller.add_sibling(id);

            // Start routes discovery and initiate alive exchange if channel established
            // with gateway
            if (is_gateway) {
                _rtab.add_gateway(id);

                std::vector<char> msg = _rtab.serialize_request(_id);
                this->enqueue_packet(id, 0, std::move(msg));

                // Send available routes to connected gateway on behalf of destination (according to
                // routing table) nodes.
                if (_is_gateway) {
                    // TODO Should all known routes be sent or only sibling nodes are sufficient?

                    _rtab.foreach_sibling_node([this, id] (node_id initiator_id) {
                        if (initiator_id != id) {
                            route_info<node_id> rinfo;
                            rinfo.initiator_id = initiator_id;
                            std::vector<char> msg = _rtab.serialize_request(_id, rinfo);
                            this->enqueue_packet(id, 0, std::move(msg));
                        }
                    });
                }
            }

            if (route_added) {
                if (_on_route_ready)
                    _on_route_ready(id, gateway_chain_type{});
            }
        });

        node->on_channel_destroyed([this] (node_id id, node_index_t /*index*/) {
            if (_on_channel_destroyed)
                _on_channel_destroyed(id);

            _rtab.remove_sibling(id);
            _alive_controller.expire(id);
        });

        if (_on_duplicate_id) {
            node->on_duplicate_id([this] (node_id id, node_index_t, socket4_addr saddr) {
               _on_duplicate_id(id, saddr);
            });
        }

        node->on_alive_received([this](node_id id, node_index_t index, alive_info<node_id> const & ainfo) {
            process_alive_received(id, index, ainfo);
        });

        node->on_unreachable_received([this] (node_id id, node_index_t index
                , unreachable_info<node_id> const & uinfo) {
            process_unreachable_received(id, index, uinfo);
        });

        node->on_route_received([this] (node_id id, node_index_t index
                , bool is_response, route_info<node_id> const & rinfo) {
            process_route_received(id, index, is_response, rinfo);
        });

        if (_on_data_received) {
            node->on_domestic_data_received(_on_data_received);

            node->on_global_data_received([this] (node_id /*id*/, int priority
                    , node_id sender_id, node_id receiver_id, std::vector<char> bytes) {
                PFS__THROW_UNEXPECTED(_id == receiver_id, "Fix meshnet::node_pool algorithm");
                _on_data_received(sender_id, priority, std::move(bytes));
            });
        }

        node->on_forward_global_packet([this] (int priority, node_id sender_id, node_id receiver_id
                , std::vector<char> packet) {
            PFS__THROW_UNEXPECTED(_id != receiver_id && _is_gateway, "Fix meshnet::node_pool algorithm");

            node_id gw_id;
            auto ptr = locate_writer(receiver_id, & gw_id);

            if (ptr != nullptr) {
                ptr->enqueue_packet(gw_id, priority, std::move(packet));
                return;
            }

            // Notify sender about unreachable destination
            _on_error(tr::f_("forward packet: {}->{} failure: node unreachable"
                , to_string(sender_id), to_string(receiver_id)));

            std::vector<char> unreach_pkt = _alive_controller.serialize_unreachable(_id
                , sender_id, receiver_id);

            ptr = locate_writer(sender_id, & gw_id);

            if (ptr != nullptr) {
                ptr->enqueue_packet(gw_id, 0, std::move(unreach_pkt));
                return;
            } else {
                // Stuck. Nothing can be done.
                _on_error(tr::f_("unable to notify sender about unreachable destination: {}->{}: no route"
                    , to_string(sender_id), to_string(receiver_id)));
            }
        });

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

    /**
     * Initiates listening on all nodes in the pool.
     *
     * @param backlog Maximum length to which the queue of pending connections may grow (see listen(2)).
     */
    void listen (int backlog = 50)
    {
        for (auto & x: _nodes)
            x->listen(backlog);
    }

    /**
     * Initiates listening on node by @a index.
     *
     * @param index Node index.
     * @param backlog Maximum length to which the queue of pending connections may grow (see listen(2)).
     */
    void listen (node_index_t index, int backlog)
    {
        auto ptr = locate_node(index);

        if (ptr == nullptr)
            return;

        ptr->listen(backlog);
    }

    /**
     * Initiates a connection to a remote host.
     *
     * @param index Node index.
     * @param remote_saddr Remote node socket address.
     * @param behind_nat Remote host is behind NAT.
     * @return @c true if connection start successfully.
     */
    bool connect_host (node_index_t index, netty::socket4_addr remote_saddr, bool behind_nat = false)
    {
        auto ptr = locate_node(index);

        if (ptr == nullptr)
            return false;

        return ptr->connect_host(remote_saddr, behind_nat);
    }

    /**
     * Initiates a connection to a remote host.
     *
     * @param index Node index
     * @param remote_saddr Remote node socket address.
     * @param local_addr Local address to bind.
     * @param behind_nat Remote host is behind NAT.
     * @return @c true if connection start successfully.
     */
    bool connect_host (node_index_t index, netty::socket4_addr remote_saddr, netty::inet4_addr local_addr
        , bool behind_nat = false)
    {
        auto ptr = locate_node(index);

        if (ptr == nullptr)
            return false;

        return ptr->connect_host(remote_saddr, local_addr, behind_nat);
    }

    void set_frame_size (node_id id, std::uint16_t frame_size)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        node_id gw_id;
        auto ptr = locate_writer(id, & gw_id);

        if (ptr == nullptr) {
            _on_error(tr::f_("node not found to set frame size: {}", to_string(id)));
            return;
        }

        ptr->set_frame_size(id, frame_size);
    }

    /**
     * Enqueues message for delivery to specified node ID @a id.
     *
     * @param id Receiver ID.
     * @param priority Message priority.
     * @param data Message content.
     * @param len Length of the message content.
     *
     * @return @c true if route found to @a id.
     */
    bool enqueue (node_id id, int priority, char const * data, std::size_t len)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        node_id gw_id;
        auto ptr = locate_writer(id, & gw_id);

        if (ptr == nullptr) {
            _on_error(tr::f_("node not found to send data to: {}", to_string(id)));
            return false;
        }

        auto packet = _rtab.serialize_message(_id, gw_id, id, data, len);
        ptr->enqueue_packet(gw_id, priority, std::move(packet));
        return true;
    }

    /**
     * Enqueues message for delivery to specified node ID @a id.
     *
     * @param id Receiver ID.
     * @param priority Message priority.
     * @param data Message content.
     *
     * @return @c true if route found to @a id.
     */
    bool enqueue (node_id id, int priority, std::vector<char> data)
    {
        return enqueue(id, priority, data.data(), data.size());
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step ()
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        unsigned int result = 0;

        for (auto & x: _nodes)
            result += x->step();

        // The presence of a gateway is an indication that `alive` packet needs to be sent.
        if (_rtab.gateway_count() > 0) {
            // Broadcast `alive` packet
            if (_alive_controller.interval_exceeded()) {
                _alive_controller.update_notification_time();
                broadcast_alive();
            }

            // Check alive expiration
            _alive_controller.check_expiration();
        }

        return result;
    }

    void run (std::chrono::milliseconds loop_interval = std::chrono::milliseconds{10})
    {
        clear_interrupted();

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

    bool is_reachable (node_id id) const
    {
        auto gwid_opt = _rtab.gateway_for(id);

        // No route or it is unreachable
        if (!gwid_opt)
            return false;

        return true;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Utility methods
    ////////////////////////////////////////////////////////////////////////////////////////////////
    static std::string stringify (gateway_chain_type const & gw_chain)
    {
        if (gw_chain.empty())
            return std::string{};

        std::string result;

        result += to_string(gw_chain[0]);

        for (std::size_t i = 1; i < gw_chain.size(); i++) {
            result += "->";
            result += to_string(gw_chain[i]);
        }

        return result;
    }

    /**
     * Dump routing table as string vector.
     *
     * @return Vector containing strings in format:
     *         "<destination node>: <gateway chain>"
     */
    std::vector<std::string> dump_routing_table () const
    {
        std::vector<std::string> result;

        _rtab.foreach_route([this, & result] (node_id dest_id, std::vector<node_id> const & gw_chain) {
            result.push_back(fmt::format("{}: {}", to_string(dest_id), stringify(gw_chain)));
        });

        return result;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
