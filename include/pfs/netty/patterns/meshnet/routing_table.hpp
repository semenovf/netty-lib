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

private:
    std::unordered_set<node_id_rep> _sibling_nodes;

    std::vector<node_id_rep> _gateways;
    std::vector<route> _routes;

    // Used to determine the route to send message.
    std::unordered_multimap<node_id_rep, route_index_type> _route_map;

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
        }

        return false;
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

        // FIXME

        // auto hops = std::numeric_limits<unsigned int>::max();
        // auto res = _routes.equal_range(id);
        //
        // // If range not found return default gateway
        // if (res.first == res.second)
        //     return default_gateway();
        //
        // pfs::optional<node_id_rep> opt_gwid;
        //
        // for (auto pos = res.first; pos != res.second; ++pos) {
        //     PFS__TERMINATE(pos->second.hops > 0, "Fix meshnet::routing_table algorithm");
        //
        //     if (hops > pos->second.hops && !pos->second.unreachable) {
        //         hops = pos->second.hops;
        //         opt_gwid = pos->second.gwid;
        //     }
        // }
        //
        // return opt_gwid;

        return pfs::nullopt;
    }

    template <typename F>
    void foreach_gateway (F && f)
    {
        for (auto const & gwid: _gateways)
            f(gwid);
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
