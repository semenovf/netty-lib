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
#include "protocol.hpp"
#include "route_info.hpp"
#include <pfs/i18n.hpp>
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

//
// A0---+       +---B0
//      |   b---|      +---D0
// A1---+   |   +---B1
//      |---a-----------d---+---D1
// A2---+   |   +---C0            |
//      |   c---|              +---D2
// A3---+       +---C1
//
// Sibling nodes (std::unordered_set) for node A0
// +----+----+----+------+
// | A1 | A2 | A3 | ...  |
// +----+----+----+------+
//
// Sibling gateways (std::vector)
//   0
// +---+-----+
// | a | ... |
// +---+-----+
//
// Gateway chains (std::vector)
//   +---+
// 0 | a |
//   +---+---+
// 1 | a | b |
//   +---+---+---+
// 2 | a | b | c |
//   +---+---+---+---+
// 3 | a | b | c | d |
//   +---+---+---+---+
//   | ...       |
//   +---+---+---+
//
// Route map (std::unordered_multimap) - mapping destination node to index of route in the route
// vector (Routes), excluding siblings.
// +----+---+
// | b  | 0 |
// +----+---+
// | c  | 1 |
// +----+---+
// | d  | 2 |
// +----+---+
// | B0 | 1 |
// +----+---+
// | B1 | 1 |
// +----+---+
// | C0 | 2 |
// +----+---+
// |  ...   |
// +----+---+
// | D2 | 3 |
// +----+---+

template <typename NodeId, typename SerializerTraits>
class routing_table
{
    using node_id = NodeId;
    using route_map_type = std::unordered_multimap<node_id, std::size_t>;

public:
    using serializer_traits_type = SerializerTraits;
    using archive_type = typename serializer_traits_type::archive_type;
    using serializer_type = typename serializer_traits_type::serializer_type;
    using gateway_chain_type = std::vector<node_id>;

private:
    std::unordered_set<node_id> _sibling_nodes;
    std::vector<node_id> _sibling_gateways;

    std::vector<gateway_chain_type> _gateway_chains;

    // Used to determine the route to send message.
    route_map_type _route_map;

public:
    routing_table () = default;
    routing_table (routing_table const &) = delete;
    routing_table (routing_table &&) = delete;
    routing_table & operator = (routing_table const &) = delete;
    routing_table & operator = (routing_table &&) = delete;

    ~routing_table () = default;

public:
    std::size_t gateway_count () const noexcept
    {
        return _sibling_gateways.size();
    }

    /**
     * Adds new sibling gateway node @a gwid.
     *
     * @return @c true if sibling gateway added or @c false if it is already exists.
     */
    bool add_sibling_gateway (node_id gwid)
    {
        // Check if already exists
        for (auto const & x: _sibling_gateways) {
            if (x == gwid)
                return false;
        }

        _sibling_gateways.push_back(gwid);
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

            PFS__THROW_UNEXPECTED(pos != reversed_gw_chain.cend(), "Fix meshnet::routing_table algorithm");

            return add_route_helper(dest, gateway_chain_type(++pos, reversed_gw_chain.cend()));
        }

        auto pos = std::find(gw_chain.cbegin(), gw_chain.cend(), gw);

        PFS__THROW_UNEXPECTED(pos != gw_chain.cend(), "Fix meshnet::routing_table algorithm");

        return add_route_helper(dest, gateway_chain_type(++pos, gw_chain.cend()));
    }

    bool is_reachable (node_id dest_id) const
    {
        return !(_sibling_nodes.find(dest_id) == _sibling_nodes.end()
            && _route_map.find(dest_id) == _route_map.end());
    }

    /**
     * Removes routes to node @a dest_id with a terminal gateway @a gw_id.
     *
     * @return Number of routes removed.
     *
     * @example Initial state: A---a---b---c---C
     *          1) if `b` disconnected from `c`: A---a---b-x-c---C, then gw_id=`b`, dest_id=`c`;
     *          2) if `c` disconnected from `C`: A---a---b---c-x-C, then gw_id=`c`, dest_id=`C`.
     */
    template <typename OnRouteLostCb, typename OnNodeUnreachableCb>
    std::size_t remove_routes (node_id gw_id, node_id dest_id
        , OnRouteLostCb && on_route_lost_cb
        , OnNodeUnreachableCb && on_node_unreachable_cb)
    {
        std::size_t n = 0;
        std::set<node_id> candidate_unreachable_nodes;

        // Called from channel destroyed callback (see node::_on_channel_destroyed)
        if (gw_id == node_id{}) {
            PFS__THROW_UNEXPECTED(is_sibling(dest_id), "Fix meshnet::routing_table algorithm");

            remove_sibling(dest_id);
            on_route_lost_cb(dest_id, 0);
            candidate_unreachable_nodes.insert(dest_id);

            ++n;
        }

        for (auto pos = _route_map.cbegin(); pos != _route_map.cend();) {
            auto const & route = _gateway_chains[pos->second];
            auto found = dest_id == pos->first
                ? true // `dest_id` is a terminal node of the route.
                // Check if `dest_id` is a gateway in the chain.
                : std::find(route.cbegin(), route.cend(), dest_id) != route.cend();

            if (found) {
                auto id = pos->first;
                auto index = pos->second;
                pos = _route_map.erase(pos);
                on_route_lost_cb(id, index + 1);
                candidate_unreachable_nodes.insert(id);
                ++n;
            } else {
                ++pos;
            }
        }

        for (auto const & x: candidate_unreachable_nodes) {
            if (!is_reachable(x))
                on_node_unreachable_cb(x);
        }

        return n;
    }

    /**
     * Iterates over all sibling gateways and call @a f for each gateway ID.
     *
     * @param f Invokable object with signature void (node_id gw_id).
     */
    template <typename F>
    void foreach_sibling_gateway (F && f) const
    {
        for (auto const & gw_id: _sibling_gateways)
            f(gw_id);
    }

    /**
     * Iterates over all sibling nodes and call @a f for each sibling node ID.
     *
     * @param f Invokable object with signature void (node_id id).
     */
    template <typename F>
    void foreach_sibling_node (F && f) const
    {
        for (auto const & x: _sibling_nodes)
            f(x);
    }

    /**
     * Iterates over all routes, include sibling nodes.
     *
     * @param f Invokable object with signature void (node_id dest, gateway_chain_type const &).
     */
    template <typename F>
    void foreach_route (F && f) const
    {
        for (auto const & x: _sibling_nodes)
            f(x, gateway_chain_type{x});

        for (auto const & x: _route_map) {
            auto index = x.second;
            f(x.first, _gateway_chains.at(index));
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

        auto & gw_chain = _gateway_chains.at(res.second);

        // Return first gateway in the chain
        return gw_chain[0];
    }

    /**
     * Returns the number of gateways in the gateway chain by @a index. Zero index indicates
     * sibling node, so result is zero.
     */
    std::size_t hops (std::size_t index)
    {
        if (index == 0)
            return 0;

        if (index > _gateway_chains.size()) {
            throw error {
                 std::make_error_code(std::errc::invalid_argument)
               , tr::f_("gateway chain index is out of bounds")
            };
        }

        return _gateway_chains[index - 1].size();
    }

    /**
     * Return gateway chain by index (first element of the value returned by add_route or add_subroute).
     * Zero index indicates sibling node, so result is empty chain.
     */
    gateway_chain_type gateway_chain_by_index (size_t index)
    {
        if (index == 0)
            return gateway_chain_type{};

        if (index > _gateway_chains.size()) {
            throw error {
                  std::make_error_code(std::errc::invalid_argument)
                , tr::f_("gateway chain index is out of bounds")
            };
        }

        return _gateway_chains[index - 1];
    }

public: // static
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Serialization methods
    ////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * Serializes initial request
     */
    static archive_type serialize_request (node_id initiator_id)
    {
        route_info<node_id> info;
        info.initiator_id = initiator_id;

        archive_type ar;
        serializer_type out {ar};
        route_packet<node_id> pkt {packet_way_enum::request, std::move(info)};
        pkt.serialize(out);
        return ar;
    }

    /**
     * Serializes request to forward.
     */
    static archive_type serialize_request (node_id gw_id, route_info<node_id> const & initial_info)
    {
        route_info<node_id> info = initial_info;
        info.route.push_back(gw_id);

        archive_type ar;
        serializer_type out {ar};
        route_packet<node_id> pkt {packet_way_enum::request, std::move(info)};
        pkt.serialize(out);
        return ar;
    }

    /**
     * Serializes initial response
     */
    static archive_type serialize_response (node_id responder_id, route_info<node_id> const & initial_info)
    {
        route_info<node_id> info = initial_info;
        info.responder_id = responder_id;

        archive_type ar;
        serializer_type out {ar};
        route_packet<node_id> pkt {packet_way_enum::response, std::move(info)};
        pkt.serialize(out);
        return ar;
    }

    /**
     * Serializes response to forward.
     */
    static archive_type serialize_response (route_info<node_id> const & initial_info)
    {
        route_info<node_id> info = initial_info;

        archive_type ar;
        serializer_type out {ar};
        route_packet<node_id> pkt {packet_way_enum::response, std::move(info)};
        pkt.serialize(out);
        return ar;
    }

    /**
     * Serializes unreachable packet
     */
    static archive_type serialize (unreachable_info<node_id> uinfo)
    {
        archive_type ar;
        serializer_type out {ar};
        unreachable_packet<node_id> pkt {std::move(uinfo)};
        pkt.serialize(out);
        return ar;
    }

private:
    std::pair<bool, std::size_t> find_route (gateway_chain_type const & r) const
    {
        for (std::size_t i = 0; i < _gateway_chains.size(); i++) {
            if (r == _gateway_chains[i])
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
            auto & r = _gateway_chains[pos->second];
            auto hops = r.size();

            PFS__THROW_UNEXPECTED(hops > 0, "Fix meshnet::routing_table algorithm");

            if (min_hops > hops /*&& r.good()*/) {
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
        PFS__THROW_UNEXPECTED(!gw_chain.empty(), "Fix meshnet::routing_table algorithm");

        auto res = find_route(gw_chain);

        // Not found
        if (!res.first) {
            _gateway_chains.push_back(std::move(gw_chain));
            auto index = _gateway_chains.size() - 1;

            _route_map.insert({dest_id, index});
            return std::make_pair(std::size_t{index + 1}, true);
        }

        // Check if record for destination node already exists
        auto range = _route_map.equal_range(dest_id);

        if (range.first != range.second) {
            // Check if route to destination already exists
            for (auto pos = range.first; pos != range.second; ++pos) {
                auto & tmp = _gateway_chains[pos->second];

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
};

} // namespace meshnet

NETTY__NAMESPACE_END
