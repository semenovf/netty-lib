////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.24 Initial version (`node_pool.hpp`).
//      2025.06.30 Added method `set_frame_size()`.
//      2025.08.08 `interruptable` inheritance.
//      2025.12.18 Renamed to `node.hpp`.
//                 `node_pool` renamed to `node`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../callback.hpp"
#include "../../interruptable.hpp"
#include "../../trace.hpp"
#include "protocol.hpp"
#include "peer_index.hpp"
#include "peer_interface.hpp"
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

#if NETTY__TELEMETRY_ENABLED
#   include "telemetry.hpp"
#endif

NETTY__NAMESPACE_BEGIN

namespace meshnet {

template <typename NodeId
    , typename RoutingTable
    , typename RecursiveWriterMutex>
class node: public interruptable
{
    using serializer_traits_type = typename RoutingTable::serializer_traits_type;
    using archive_type = typename serializer_traits_type::archive_type;
    using serializer_type = typename serializer_traits_type::serializer_type;
    using peer_interface_type = peer_interface<NodeId, archive_type>;
    using peer_interface_ptr = std::unique_ptr<peer_interface_type>;
    using routing_table_type = RoutingTable;
    using recursive_mutex_type = RecursiveWriterMutex;

#if NETTY__TELEMETRY_ENABLED
    using shared_telemetry_producer_type = std::shared_ptr<telemetry_producer<serializer_traits_type>>;
#endif

public:
    using node_id = NodeId;
    using address_type = node_id;
    using gateway_chain_type = typename routing_table_type::gateway_chain_type;

private:
    node_id _id;

    // True if the node is a gateway
    bool _is_gateway {false};

    // There will rarely be more than dozens endpoints, so vector is a optimal choice
    std::vector<peer_interface_ptr> _endpoints;

    routing_table_type _rtab;

    // Writer mutex to protect sending
    recursive_mutex_type _writer_mtx;

    // Thread where node created
    std::thread::id _thread_id;

#if NETTY__TELEMETRY_ENABLED
    shared_telemetry_producer_type _telemetry_producer;
#endif

private:
    callback_t<void (std::string const &)> _on_error
        = [] (std::string const & errstr) { LOGE(TAG, "{}", errstr); };

    callback_t<void (peer_index_t, node_id, bool)> _on_channel_established;
    callback_t<void (node_id)> _on_channel_destroyed;
    callback_t<void (node_id, socket4_addr)> _on_duplicate_id;
    callback_t<void (node_id, std::size_t)> _on_route_ready;
    callback_t<void (node_id, std::size_t)> _on_route_lost;
    callback_t<void (node_id)> _on_node_unreachable;
    callback_t<void (node_id, int, archive_type)> _on_data_received;

public:
    node (node_id id, bool is_gateway = false)
        : interruptable()
        , _id(id)
        , _is_gateway(is_gateway)
        , _thread_id(std::this_thread::get_id())
    {}

#if NETTY__TELEMETRY_ENABLED
    node (node_id id, bool is_gateway, shared_telemetry_producer_type telemetry_producer)
        : node(id, is_gateway)
    {
        _telemetry_producer = telemetry_producer;
    }
#endif

    node (node const &) = delete;
    node (node &&) = delete;
    node & operator = (node const &) = delete;
    node & operator = (node &&) = delete;

    ~node ()
    {
        if (!_endpoints.empty()) {
            for (auto & x : _endpoints)
                x->clear_channels();
        }
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
    node & on_error (F && f)
    {
        _on_error = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when connection established with the remote node.
     *
     * @details Callback @a f signature must match:
     *          void (peer_index_t, node_id peer_id, bool is_gateway)
     */
    template <typename F>
    node & on_channel_established (F && f)
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
    node & on_channel_destroyed (F && f)
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
    node & on_duplicate_id (F && f)
    {
        _on_duplicate_id = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when some route ready by request or response.
     *
     * @details Callback @a f signature must match:
     *          void (node_id id, std::size_t route_index)
     *          `route_index` has a special case of zero occurs when `id` is a sibling node.
     */
    template <typename F>
    node & on_route_ready (F && f)
    {
        _on_route_ready = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when route is lost.
     *
     * @details Callback @a f signature must match:
     *          void (node_id id, std::size_t route_index).
     *          `route_index` has a special case of zero occurs when `id` is a sibling node.
     */
    template <typename F>
    node & on_route_lost (F && f)
    {
        _on_route_lost = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when node is unreachable (no routes found).
     *
     * @details Callback @a f signature must match void (node_id unreachable_id).
     */
    template <typename F>
    node & on_node_unreachable (F && f)
    {
        _on_node_unreachable = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when message received (domestic or global)
     *
     * @details Callback @a f signature must match:
     *          void (node_id sender_id, int priority, archive_type bytes)
     */
    template <typename F>
    node & on_data_received (F && f)
    {
        _on_data_received = std::forward<F>(f);
        return *this;
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

    /**
     * Adds new endpoint to the node with specified listeners.
     */
    template <typename Peer, typename ListenerIt>
    peer_index_t add (ListenerIt first, ListenerIt last, error * perr = nullptr)
    {
        PFS__ASSERT(_thread_id == std::this_thread::get_id(), "Peer must be added before run() call");

#if NETTY__TELEMETRY_ENABLED
        auto ep = Peer::template make_interface(_id, _is_gateway, _telemetry_producer);
#else
        auto ep = Peer::template make_interface(_id, _is_gateway);
#endif
        error err;

        for (ListenerIt pos = first; pos != last; ++pos) {
            ep->add_listener(*pos, & err);

            if (err)
                pfs::throw_or(perr, std::move(err));
        }

        //
        // Assign endpoint callbacks
        //
        ep->on_error(_on_error);

        ep->on_channel_established([this] (peer_index_t index, node_id peer_id, bool is_gateway) {
            _on_channel_established(index, peer_id, is_gateway);

            // Add direct route
            auto route_added = _rtab.add_sibling(peer_id);

            // Channel established with gateway
            if (is_gateway) {
                _rtab.add_sibling_gateway(peer_id);

                archive_type msg = _rtab.serialize_request(_id);
                this->enqueue_packet(peer_id, 0, std::move(msg));

                // Send available routes to connected gateway on behalf of destination (according to
                // routing table) nodes.
                if (_is_gateway) {
                    // TODO Should all known routes be sent or only sibling nodes are sufficient?

                    _rtab.foreach_sibling_node([this, peer_id] (node_id initiator_id) {
                        if (initiator_id != peer_id) {
                            route_info<node_id> rinfo;
                            rinfo.initiator_id = initiator_id;
                            archive_type msg1 = _rtab.serialize_request(_id, rinfo);
                            this->enqueue_packet(peer_id, 0, std::move(msg1));
                        }
                    });
                }
            }

            if (route_added) {
                if (_on_route_ready) {
                    NETTY__TRACE(MESHNET_TAG, "route ready: {} (hops={})", to_string(peer_id), 0);
                    _on_route_ready(peer_id, 0);
                }
            }
        });

        ep->on_channel_destroyed([this] (peer_index_t index, node_id peer_id) {
            if (_on_channel_destroyed)
                _on_channel_destroyed(peer_id);

            auto n = _rtab.remove_routes(node_id{}, peer_id
                , [this] (node_id dest_id, std::size_t gw_chain_index) {
                    if (_on_route_lost)
                        _on_route_lost(dest_id, gw_chain_index);
                }
                , [this] (node_id dest_id) {
                    if (_on_node_unreachable)
                        _on_node_unreachable(dest_id);
                }
            );

            if (n > 0) {
                if (_is_gateway)
                    broadcast_unreachable(peer_id);
            }

            // FIXME REMOVE
            // if (_on_route_lost)
            //     _on_route_lost(peer_id, 0);
            //
            // // Check if there are no other routes to `peer_id` node and notify about it.
            // if (_on_node_unreachable) {
            //     if (!_rtab.is_reachable(peer_id))
            //         _on_node_unreachable(peer_id);
            // }
        });

        if (_on_duplicate_id) {
            ep->on_duplicate_id([this] (peer_index_t, node_id id, socket4_addr saddr) {
               _on_duplicate_id(id, saddr);
            });
        }

        ep->on_unreachable_received([this] (peer_index_t index, node_id id
                , unreachable_info<node_id> const & uinfo) {
            process_unreachable_received(index, id, uinfo);
        });

        ep->on_route_received([this] (peer_index_t index, node_id id
                , bool is_response, route_info<node_id> const & rinfo) {
            process_route_received(index, id, is_response, rinfo);
        });

        if (_on_data_received) {
            ep->on_domestic_data_received(_on_data_received);

            ep->on_global_data_received([this] (node_id /*id*/, int priority
                    , node_id sender_id, node_id receiver_id, archive_type bytes) {
                PFS__THROW_UNEXPECTED(_id == receiver_id, "Fix meshnet::node algorithm");
                _on_data_received(sender_id, priority, std::move(bytes));
            });
        }

        ep->on_forward_global_packet([this] (int priority, node_id sender_id, node_id receiver_id
                , archive_type packet) {
            PFS__THROW_UNEXPECTED(_id != receiver_id && _is_gateway, "Fix meshnet::node algorithm");

            node_id gw_id;
            auto ptr = locate_writer(receiver_id, & gw_id);

            if (ptr != nullptr) {
                ptr->enqueue_packet(gw_id, priority, std::move(packet));
                return;
            }

            _on_error(tr::f_("forward packet: {}->{} failure: node unreachable"
                , to_string(sender_id), to_string(receiver_id)));

            // No need to notify sender about unreachable destination.
            // The corresponding unreachable_packet must be sent at the moment the channel destroyed.
        });

        _endpoints.push_back(std::move(ep));

        peer_index_t index = static_cast<peer_index_t>(_endpoints.size());
        _endpoints.back()->set_index(index);
        return index;
    }

    /**
     * Adds new endpoint to the node with specified listeners.
     */
    template <typename Peer>
    peer_index_t add (std::vector<socket4_addr> const & listener_saddrs, error * perr = nullptr)
    {
        return add<Peer>(listener_saddrs.begin(), listener_saddrs.end(), perr);
    }

    /**
     * Adds new endpoint to the node with specified listeners.
     */
    template <typename Peer>
    peer_index_t add (std::initializer_list<socket4_addr> const & listener_saddrs, error * perr = nullptr)
    {
        return add<Peer>(listener_saddrs.begin(), listener_saddrs.end(), perr);
    }

    /**
     * Initiates listening on all peers in the node.
     *
     * @param backlog Maximum length to which the queue of pending connections may grow (see listen(2)).
     */
    void listen (int backlog = 50)
    {
        std::unique_lock<recursive_mutex_type> locker{_writer_mtx};

        for (auto & x: _endpoints)
            x->listen(backlog);
    }

    /**
     * Initiates a connection to a peer.
     *
     * @param index Peer index.
     * @param remote_saddr Remote node socket address.
     * @param behind_nat Remote host is behind NAT.
     * @return @c true if connection start successfully.
     */
    bool connect_peer (peer_index_t index, netty::socket4_addr remote_saddr, bool behind_nat = false)
    {
        PFS__ASSERT(_thread_id == std::this_thread::get_id()
            , "connect_host() must be call in the same thread where node has been created");

        auto ep = locate_endpoint(index);

        if (ep == nullptr)
            return false;

        return ep->connect(remote_saddr, behind_nat);
    }

    /**
     * Initiates a connection to a peer.
     *
     * @param index Node index in the pool.
     * @param remote_saddr Remote node socket address.
     * @param local_addr Local address to bind.
     * @param behind_nat Remote host is behind NAT.
     * @return @c true if connection start successfully.
     */
    bool connect_peer (peer_index_t index, netty::socket4_addr remote_saddr, netty::inet4_addr local_addr
        , bool behind_nat = false)
    {
        PFS__ASSERT(_thread_id == std::this_thread::get_id()
            , "connect_peer() must be call in the same thread where node has been created");

        auto ep = locate_endpoint(index);

        if (ep == nullptr)
            return false;

        return ep->connect(remote_saddr, local_addr, behind_nat);
    }

    void disconnect (peer_index_t index, node_id peer_id)
    {
        std::unique_lock<recursive_mutex_type> locker{_writer_mtx};

        auto ep = locate_endpoint(index);

        if (ep == nullptr) {
            _on_error(tr::f_("unable to disconnect from: peer index={}, peer id={}", index, peer_id));
            return;
        }

        ep->disconnect(peer_id);
    }

    /**
     * Sets maximum frame size @a frame_size for exchange with node specified by
     * identifier @a peer_id.
     */
    void set_frame_size (peer_index_t index, node_id peer_id, std::uint16_t frame_size)
    {
        std::unique_lock<recursive_mutex_type> locker{_writer_mtx};

        auto ep = locate_endpoint(index);

        if (ep == nullptr) {
            _on_error(tr::f_("unable to set frame size: peer index={}, peer id={}", index, peer_id));
            return;
        }

        ep->set_frame_size(peer_id, frame_size);
    }

    /**
     * Enqueues message for delivery to specified node ID @a id.
     *
     * @param receiver_id Receiver ID.
     * @param priority Message priority.
     * @param data Message content.
     * @param len Length of the message content.
     *
     * @return @c true if route found to @a receiver_id.
     */
    bool enqueue (node_id receiver_id, int priority, char const * data, std::size_t len)
    {
        std::unique_lock<recursive_mutex_type> locker{_writer_mtx};

        node_id gw_id;
        auto wr = locate_writer(receiver_id, & gw_id);

        if (wr == nullptr) {
            _on_error(tr::f_("peer not found to send data to: {}", to_string(receiver_id)));
            return false;
        }

        // Serialize initial custom message.

        archive_type ar;
        serializer_type out {ar};

        // Domestic exchange
        if (gw_id == receiver_id) {
            ddata_packet pkt;
            pkt.serialize(out, data, len);
        } else {
            // Intersegment exchange
            gdata_packet<node_id> pkt {_id, receiver_id};
            pkt.serialize(out, data, len);
        }

        wr->enqueue_packet(gw_id, priority, std::move(ar));
        return true;
    }

    /**
     * Enqueues message for delivery to specified node ID @a id.
     *
     * @param receiver_id Receiver ID.
     * @param priority Message priority.
     * @param data Message content.
     *
     * @return @c true if route found to @a receiver_id.
     */
    bool enqueue (node_id receiver_id, int priority, archive_type const & data)
    {
        return enqueue(receiver_id, priority, data.data(), data.size());
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step ()
    {
        std::unique_lock<recursive_mutex_type> locker{_writer_mtx};

        unsigned int result = 0;

        for (auto & x: _endpoints)
            result += x->step();

        return result;
    }

    void run (std::chrono::milliseconds loop_interval = std::chrono::milliseconds{10})
    {
        clear_interrupted();

        while (!interrupted()) {
            pfs::countdown_timer<std::milli> countdown_timer {loop_interval};
            auto n = step();

            if (n == 0)
                std::this_thread::sleep_for(countdown_timer.remain());
        }
    }

    bool is_reachable (node_id id) const
    {
        return _rtab.is_reachable(id);
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

private:
    peer_interface_type * locate_endpoint (node_id id)
    {
        if (_endpoints[0]->id() == id)
            return &*_endpoints[0];

        for (int i = 1; i < _endpoints.size(); i++) {
            if (_endpoints[i]->id() == id)
                return &*_endpoints[i];
        }

        return nullptr;
    }

    peer_interface_type * locate_endpoint (peer_index_t index)
    {
        if (index < 1 || index > _endpoints.size()) {
            _on_error(tr::f_("peer index is out of bounds: {}", index));
            return nullptr;
        }

        return &*_endpoints[index - 1];
    }

    peer_interface_type * locate_writer (node_id id, node_id * gw_id = nullptr)
    {
        if (_endpoints.empty())
            return nullptr;

        auto gw_id_opt = _rtab.gateway_for(id);

        // No route or it is unreachable
        if (!gw_id_opt)
            return nullptr;

        if (gw_id != nullptr)
            *gw_id = *gw_id_opt;

        if (_endpoints[0]->has_writer(*gw_id_opt))
            return &*_endpoints[0];

        for (int i = 1; i < _endpoints.size(); i++) {
            if (_endpoints[i]->has_writer(*gw_id_opt)) {
                return &*_endpoints[i];
            }
        }

        return nullptr;
    }

    bool enqueue_packet (node_id id, int priority, archive_type data)
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

    void process_unreachable_received (peer_index_t /*idx*/, node_id peer_id
        , unreachable_info<node_id> const & uinfo)
    {
        // Prevent cycling
        if (_id == uinfo.gw_id)
            return;

        // `uinfo.id` node cannot be reached through the gateway `uinfo.gw_id`.
        // Disable all routes containing the specified subchain.
        auto n = _rtab.remove_routes(uinfo.gw_id, uinfo.id
            , [this] (node_id dest_id, std::size_t gw_chain_index) {
                if (_on_route_lost)
                    _on_route_lost(dest_id, gw_chain_index);
            }
            , [this] (node_id dest_id) {
                if (_on_node_unreachable)
                    _on_node_unreachable(dest_id);
            }
        );

        if (n > 0) {
            // Forward packet to sibling nodes excluding `peer_id`.
            if (_is_gateway) {
                archive_type ar = _rtab.serialize(uinfo);
                forward_packet(peer_id, ar);
            }
        }
    }

    void process_route_received (peer_index_t /*idx*/, node_id id, bool is_response
        , route_info<node_id> const & rinfo)
    {
        bool new_route_added = false;
        std::size_t gw_chain_index{0};
        std::uint16_t hops = 0;
        node_id dest_id{};
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
                    PFS__THROW_UNEXPECTED(hops > 0, "Fix meshnet::node algorithm");

                    // Find this gateway
                    auto opt_index = rinfo.gateway_index(_id);

                    PFS__THROW_UNEXPECTED(opt_index, "Fix meshnet::node algorithm");

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
                    }

                    // Serialize response and send to previous gateway (if index > 0)
                    // or to the initiator node
                    archive_type msg = _rtab.serialize_response(rinfo);

                    if (index > 0) {
                        auto addressee_id = rinfo.route[index - 1];
                        enqueue_packet(addressee_id, 0, std::move(msg));
                    } else {
                        auto addressee_id = rinfo.initiator_id;
                        enqueue_packet(addressee_id, 0, std::move(msg));
                    }
                }
            } else {
                PFS__THROW_UNEXPECTED(false, "Fix meshnet::node algorithm");
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
                    PFS__THROW_UNEXPECTED(hops > 0, "Fix meshnet::node algorithm");
                    auto res = _rtab.add_route(dest_id, rinfo.route, reverse_order);
                    gw_chain_index = res.first;
                    new_route_added = res.second;
                }

                // Initiate response and transmit it by the reverse route
                archive_type msg = _rtab.serialize_response(_id, rinfo);
                enqueue_packet(id, 0, std::move(msg));

                // Forward request to nearest nodes if this gateway is not present in the received route
                // to prevent cycling.
                if (_is_gateway) {
                    auto opt_index = rinfo.gateway_index(_id);

                    if (!opt_index) {
                        archive_type msg1 = _rtab.serialize_request(_id, rinfo);
                        forward_packet(id, std::move(msg1));
                    }
                }
            }
        }

        if (new_route_added) {
            PFS__THROW_UNEXPECTED(_id != dest_id, "Fix meshnet::node algorithm");

            if (_on_route_ready) {
                NETTY__TRACE(MESHNET_TAG, "route ready: {} (hops={}, gw_chain_index={})"
                    , to_string(dest_id), _rtab.hops(gw_chain_index), gw_chain_index);

                _on_route_ready(dest_id, gw_chain_index);
            }
        }
    }

    /**
     * Forward special packets (route and unreachable) to nearest nodes excluding node identified
     * by @a sender_id.
     *
     * @param ar Serialized packet.
     */
    void forward_packet (node_id sender_id, archive_type const & ar)
    {
        for (peer_index_t i = 0; i < _endpoints.size(); i++)
            _endpoints[i]->enqueue_forward_packet(sender_id, 0, ar.data(), ar.size());
    }

    /**
     * Broadcasts unreachable packet (used by gateways only).
     */
    void broadcast_unreachable (node_id unreachable_id)
    {
        unreachable_info<node_id> uinfo { _id, unreachable_id };
        archive_type ar = _rtab.serialize(std::move(uinfo));

        for (peer_index_t i = 0; i < _endpoints.size(); i++)
            _endpoints[i]->enqueue_broadcast_packet(0, ar.data(), ar.size());
    }
};

} // namespace meshnet

NETTY__NAMESPACE_END
