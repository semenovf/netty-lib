////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../error.hpp"
#include "../../namespace.hpp"
#include "../serializer_traits.hpp"
#include "node_id_rep.hpp"
#include "protocol.hpp"
#include "route_info.hpp"
#include "route.hpp"
#include <pfs/i18n.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename SerializerTraits>
class routing_table
{
    using serializer_traits = SerializerTraits;
    using route_index_type = int;
    using route_map_type = std::unordered_multimap<node_id_rep, route_index_type>;

private:
    std::unordered_set<node_id_rep> _sibling_nodes;

    std::vector<node_id_rep> _gateways;
    std::vector<route> _routes;

    // Used to determine the route to send message.
    route_map_type _route_map;

public:
    routing_table () = default;
    routing_table (routing_table const &) = delete;
    routing_table (routing_table &&) = delete;
    routing_table & operator = (routing_table const &) = delete;
    routing_table & operator = (routing_table &&) = delete;

    ~routing_table () = default;

private:
    int find_route (route const & r) const
    {
        for (int i = 0; i < _routes.size(); i++) {
            if (r.equals_to(_routes[i]))
                return i;
        }

        return -1;
    }

    template <typename It>
    bool add_route (node_id_rep dest, It first, It last)
    {
        route r {first, last};

        auto index = find_route(r);

        // Not found
        if (index < 0) {
            _routes.push_back(std::move(r));
            index = static_cast<int>(_routes.size() - 1);

            _route_map.insert({dest, index});
            return true;
        } else {
            // Check if record for destination node already exists
            auto res = _route_map.equal_range(dest);

            if (res.first != res.second) {
                // Check if route to destination already exists
                for (auto pos = res.first; pos != res.second; ++pos) {
                    auto & tmp = _routes[pos->second];

                    // Already exists
                    if (tmp.equals_to(tmp))
                        return false;
                }
            }

            _route_map.insert({dest, index});
            return true;
        }

        return false;
    }

    bool is_sibling (node_id_rep id_rep) const
    {
        return _sibling_nodes.find(id_rep) != _sibling_nodes.end();
    }

public:
    std::size_t gateway_count () const noexcept
    {
        return _gateways.size();
    }

    /**
     * Adds new gateway node @a id_rep.
     *
     * @return @c true if gateway node added or @c false if gateway node already exists
     */
    bool add_gateway (node_id_rep id_rep)
    {
        // Check if already exists
        for (auto const & x: _gateways) {
            if (x == id_rep)
                return false;
        }

        _gateways.push_back(id_rep);
        return true;
    }

    /**
     * Adds new sibling node @a id_rep.
     *
     * @return @c true if sibling node added or @c false if sibling node already exists
     */
    bool add_sibling (node_id_rep id_rep)
    {
        // Remove all non-direct routes between sibling nodes
        _route_map.erase(id_rep);

        auto res = _sibling_nodes.insert(id_rep);
        return res.second;
    }

    void remove_sibling (node_id_rep id)
    {
        _sibling_nodes.erase(id);
    }

    /**
     * Adds new route to the destination node @a dest.
     *
     * @return @c true if new route added or @c false if route already exists
     */
    bool add_route (node_id_rep dest, route_info const & rinfo, route_order_enum order)
    {
        if (is_sibling(dest))
            return false;

        route r;

        if (order == route_order_enum::reverse) {
            auto rr = rinfo.reverse_route();
            return add_route(dest, rr.cbegin(), rr.cend());
        }

        return add_route(dest, rinfo.route.cbegin(), rinfo.route.cend());
    }

    /**
     * Adds new route to the destination node @a dest constructed from sub-route.
     *
     * @return @c true if new route added or @c false if route already exists
     */
    bool add_subroute (node_id_rep dest, node_id_rep gw, route_info const & rinfo
        , route_order_enum order)
    {
        if (is_sibling(dest))
            return false;

        if (order == route_order_enum::reverse) {
            auto rr = rinfo.reverse_route();

            auto pos = std::find(rr.cbegin(), rr.cend(), gw);

            PFS__TERMINATE(pos != rr.cend(), "Fix meshnet::routing_table algorithm");

            return add_route(dest, ++pos, rr.cend());
        }

        auto pos = std::find(rinfo.route.cbegin(), rinfo.route.cend(), gw);

        PFS__TERMINATE(pos != rinfo.route.cend(), "Fix meshnet::routing_table algorithm");

        return add_route(dest, ++pos, rinfo.route.end());
    }

    /**
     * Searches gateway for destination node @a id.
     *
     * @details Preference is given to a route with a low value of hops and its reachability.
     */
    pfs::optional<node_id_rep> gateway_for (node_id_rep id_rep)
    {
        // Check if direct access
        if (_sibling_nodes.find(id_rep) != _sibling_nodes.cend())
            return id_rep;

        auto min_hops = std::numeric_limits<std::size_t>::max();
        auto res = _route_map.equal_range(id_rep);

        // Not found
        if (res.first == res.second)
            return pfs::nullopt;

        pfs::optional<node_id_rep> opt_gwid;

        for (auto pos = res.first; pos != res.second; ++pos) {
            auto & r = _routes[pos->second];
            auto hops = r.hops();

            PFS__TERMINATE(hops > 0, "Fix meshnet::routing_table algorithm");

            if (min_hops > hops && r.good()) {
                min_hops = hops;
                opt_gwid = r.gateway();
            }
        }

        return opt_gwid;
    }

    template <typename F>
    void foreach_gateway (F && f)
    {
        for (auto const & gwid: _gateways)
            f(gwid);
    }

    template <typename F>
    void foreach_sibling_node (F && f)
    {
        for (auto const & x: _sibling_nodes)
            f(x);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Serialization methods
    ////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * Serialize initial request
     */
    std::vector<char> serialize_request (node_id_rep initiator_id)
    {
        auto out = serializer_traits::make_serializer();
        route_packet pkt {packet_way_enum::request};
        pkt.rinfo.initiator_id = initiator_id;
        pkt.serialize(out);
        return out.take();
    }

    /**
     * Serialize request to forward.
     */
    std::vector<char> serialize_request (node_id_rep gwid, route_info const & rinfo)
    {
        auto out = serializer_traits::make_serializer();
        route_packet pkt {packet_way_enum::request};
        pkt.rinfo = rinfo;
        pkt.rinfo.route.push_back(gwid);
        pkt.serialize(out);
        return out.take();
    }

    /**
     * Serialize initial response
     */
    std::vector<char> serialize_response (node_id_rep responder_id, route_info const & rinfo)
    {
        auto out = serializer_traits::make_serializer();
        route_packet pkt {packet_way_enum::response};
        pkt.rinfo = rinfo;
        pkt.rinfo.responder_id = responder_id;
        pkt.serialize(out);
        return out.take();
    }

    /**
     * Serialize response to forward.
     */
    std::vector<char> serialize_response (route_info const & rinfo)
    {
        auto out = serializer_traits::make_serializer();
        route_packet pkt {packet_way_enum::response};
        pkt.rinfo = rinfo;
        pkt.serialize(out);
        return out.take();
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
