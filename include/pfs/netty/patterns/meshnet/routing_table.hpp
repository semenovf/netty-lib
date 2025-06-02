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
#include "protocol.hpp"
#include "route_info.hpp"
#include <pfs/i18n.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdTraits, typename SerializerTraits>
class routing_table
{
    using node_id_traits = NodeIdTraits;
    using node_id = typename node_id_traits::type;
    using serializer_traits = SerializerTraits;
    using route_map_type = std::unordered_multimap<node_id, std::size_t>;

public:
    using gateway_chain_type = std::vector<node_id>;

private:
    std::unordered_set<node_id> _sibling_nodes;

    std::vector<node_id> _gateways;
    std::vector<gateway_chain_type> _routes;

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
    std::pair<bool, std::size_t> find_route (gateway_chain_type const & r) const
    {
        for (std::size_t i = 0; i < _routes.size(); i++) {
            if (r == _routes[i])
                return std::make_pair(true, i);
        }

        return std::make_pair(false, std::size_t{0});
    }

    /**
     * Find route for node @a id with minimim hops (number of gateways).
     */
    std::pair<bool, std::size_t> find_optimal_route_for (node_id id) const
    {
        auto min_hops = std::numeric_limits<std::size_t>::max();
        auto res = _route_map.equal_range(id);

        // Not found
        if (res.first == res.second)
            return std::make_pair(false, std::size_t{0});

        std::size_t index = 0;

        for (auto pos = res.first; pos != res.second; ++pos) {
            auto & r = _routes[pos->second];
            auto hops = r.size();

            PFS__TERMINATE(hops > 0, "Fix meshnet::routing_table algorithm");

            if (min_hops > hops /*&& r.good()*/) { // FIXME Need the recognition of unreachable routes
                min_hops = hops;
                index = pos->second;
            }
        }

        // Not found
        if (min_hops == std::numeric_limits<std::size_t>::max())
            return std::make_pair(false, std::size_t{0});;

        // Found
        return std::make_pair(true, index);
    }

    std::pair<std::size_t, bool>
    add_route_helper (node_id dest_id, gateway_chain_type gw_chain)
    {
        PFS__TERMINATE(!gw_chain.empty(), "Fix meshnet::routing_table algorithm");

        auto res = find_route(gw_chain);

        // Not found
        if (!res.first) {
            _routes.push_back(std::move(gw_chain));
            auto index = _routes.size() - 1;

            _route_map.insert({dest_id, index});
            return std::make_pair(std::size_t{index + 1}, true);
        }

        // Check if record for destination node already exists
        auto range = _route_map.equal_range(dest_id);

        if (range.first != range.second) {
            // Check if route to destination already exists
            for (auto pos = range.first; pos != range.second; ++pos) {
                auto & tmp = _routes[pos->second];

                // Already exists
                if (tmp == gw_chain)
                    return std::make_pair(std::size_t{pos->second + 1}, false);
            }
        }

        auto index = res.second;
        _route_map.insert({dest_id, index});
        return std::make_pair(std::size_t{index + 1}, true);
    }

    bool is_sibling (node_id id) const
    {
        return _sibling_nodes.find(id) != _sibling_nodes.end();
    }

    static gateway_chain_type reverse_gateway_chain (gateway_chain_type const & gw_chain)
    {
        gateway_chain_type reversed_gw_chain(gw_chain.size());
        std::reverse_copy(gw_chain.begin(), gw_chain.end(), reversed_gw_chain.begin());
        return reversed_gw_chain;
    }

public:
    std::size_t gateway_count () const noexcept
    {
        return _gateways.size();
    }

    /**
     * Adds new gateway node @a id.
     *
     * @return @c true if gateway node added or @c false if gateway node already exists
     */
    bool add_gateway (node_id gwid)
    {
        // Check if already exists
        for (auto const & x: _gateways) {
            if (x == gwid)
                return false;
        }

        _gateways.push_back(gwid);
        return true;
    }

    /**
     * Adds new sibling node @a id.
     *
     * @return @c true if sibling node added or @c false if sibling node already exists
     */
    bool add_sibling (node_id id)
    {
        // Remove all non-direct routes between sibling nodes
        _route_map.erase(id);

        auto res = _sibling_nodes.insert(id);
        return res.second;
    }

    void remove_sibling (node_id id)
    {
        _sibling_nodes.erase(id);
    }

    /**
     * Adds new route to the destination node @a dest.
     *
     * @return A pair consisting of the index of a newly added route (or the index of a route that
     *         already exists) and a bool value set to @c true if and only if route adding took
     *         place. Zero index indicates sibling node.
     */
    std::pair<std::size_t, bool>
    add_route (node_id dest, gateway_chain_type const & gw_chain, bool reverse_order = false)
    {
        if (is_sibling(dest))
            return std::make_pair(std::size_t{0}, false);

        if (reverse_order) {
            auto reversed_gw_chain = reverse_gateway_chain(gw_chain);
            return add_route_helper(dest, std::move(reversed_gw_chain));
        }

        return add_route_helper(dest, gw_chain);
    }

    /**
     * Adds new route to the destination node @a dest constructed from sub-route.
     *
     * @return See add_route.
     */
    std::pair<std::size_t, bool>
    add_subroute (node_id dest, node_id gw, gateway_chain_type const & gw_chain, bool reverse_order = false)
    {
        if (is_sibling(dest))
            return std::make_pair(std::size_t{0}, false);

        if (reverse_order) {
            auto reversed_gw_chain = reverse_gateway_chain(gw_chain);
            auto pos = std::find(reversed_gw_chain.cbegin(), reversed_gw_chain.cend(), gw);

            PFS__TERMINATE(pos != reversed_gw_chain.cend(), "Fix meshnet::routing_table algorithm");

            return add_route_helper(dest, gateway_chain_type(++pos, reversed_gw_chain.cend()));
        }

        auto pos = std::find(gw_chain.cbegin(), gw_chain.cend(), gw);

        PFS__TERMINATE(pos != gw_chain.cend(), "Fix meshnet::routing_table algorithm");

        return add_route_helper(dest, gateway_chain_type(++pos, gw_chain.cend()));
    }

    void remove_route (node_id dest_id, node_id gw_id)
    {
        auto res = _route_map.equal_range(dest_id);

        // Not found
        if (res.first == res.second)
            return;

        for (auto pos = res.first; pos != res.second; ) {
            auto & r = _routes[pos->second];
            auto gw_pos = std::find(r.begin(), r.end(), gw_id);

            if (gw_pos != r.end()) {
                pos = _route_map.erase(pos);
            } else {
                ++pos;
            }
        }
    }

    template <typename F>
    void foreach_gateway (F && f) const
    {
        for (auto const & gw_id: _gateways)
            f(gw_id);
    }

    template <typename F>
    void foreach_sibling_node (F && f) const
    {
        for (auto const & x: _sibling_nodes)
            f(x);
    }

    /**
     * Iterate over all routes, include sibling nodes.
     *
     * @param f Invokable object with signature void (node_id dest, gateway_chain_type const &)
     */
    template <typename F>
    void foreach_route (F && f) const
    {
        for (auto const & x: _sibling_nodes)
            f(x, gateway_chain_type{x});

        for (auto const & x: _route_map) {
            auto index = x.second;
            f(x.first, _routes.at(index));
        }
    }

    /**
     * Searches gateway for destination node @a id.
     *
     * @details Preference is given to a route with a low value of hops and its reachability.
     */
    pfs::optional<node_id> gateway_for (node_id id) const
    {
        if (is_sibling(id))
            return id;

        auto res = find_optimal_route_for(id);

        if (!res.first)
            return pfs::nullopt;

        auto & gw_chain = _routes[res.second];

        // Return first gateway in the chain
        return gw_chain[0];
    }

    /**
     * Return gateway chain by index (first element of the value returned by add_route or add_subroute).
     * Zero index indicates sibling node, so result is empty chain.
     */
    gateway_chain_type gateway_chain_by_index (size_t index)
    {
        if (index == 0)
            return gateway_chain_type{};

        if (index > _routes.size()) {
            throw error {
                  std::make_error_code(std::errc::invalid_argument)
                , tr::f_("gateway chain index is out of bounds")
            };
        }

        return _routes[index - 1];
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Serialization methods
    ////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * Serializes initial request
     */
    std::vector<char> serialize_request (node_id initiator_id)
    {
        auto out = serializer_traits::make_serializer();
        route_packet<node_id> pkt {packet_way_enum::request};
        pkt.rinfo.initiator_id = initiator_id;
        pkt.serialize(out);
        return out.take();
    }

    /**
     * Serializes request to forward.
     */
    std::vector<char> serialize_request (node_id gw_id, route_info<node_id> const & rinfo)
    {
        auto out = serializer_traits::make_serializer();
        route_packet<node_id> pkt {packet_way_enum::request};
        pkt.rinfo = rinfo;
        pkt.rinfo.route.push_back(gw_id);
        pkt.serialize(out);
        return out.take();
    }

    /**
     * Serializes initial response
     */
    std::vector<char> serialize_response (node_id responder_id, route_info<node_id> const & rinfo)
    {
        auto out = serializer_traits::make_serializer();
        route_packet<node_id> pkt {packet_way_enum::response};
        pkt.rinfo = rinfo;
        pkt.rinfo.responder_id = responder_id;
        pkt.serialize(out);
        return out.take();
    }

    /**
     * Serializes response to forward.
     */
    std::vector<char> serialize_response (route_info<node_id> const & rinfo)
    {
        auto out = serializer_traits::make_serializer();
        route_packet<node_id> pkt {packet_way_enum::response};
        pkt.rinfo = rinfo;
        pkt.serialize(out);
        return out.take();
    }

    /**
     * Serializes initial custom message.
     */
    std::vector<char> serialize_message (node_id sender_id, node_id gw_id, node_id receiver_id
        , bool force_checksum, char const * data, std::size_t len )
    {
        // Enough space for packet header -------------------v
        auto out = serializer_traits::make_serializer(len + 64);

        // Domestic exchange
        if (gw_id == receiver_id) {
            ddata_packet pkt {force_checksum};
            pkt.serialize(out, data, len);
            return out.take();
        }

        // Intersegment exchange
        gdata_packet<node_id> pkt {sender_id, receiver_id, force_checksum};
        pkt.serialize(out, data, len);
        return out.take();
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
