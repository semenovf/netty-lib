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

public:
    std::size_t gateway_count () const noexcept
    {
        return _gateways.size();
    }

    void append_gateway (node_id_rep id)
    {
        // Check if already exists
        for (auto const & x: _gateways) {
            if (x == id)
                return;
        }

        _gateways.push_back(id);
    }

    void add_sibling (node_id_rep const & id)
    {
        _sibling_nodes.insert(id);
    }

    void remove_sibling (node_id_rep id)
    {
        _sibling_nodes.erase(id);
    }

    /**
     * Adds new route to the destination node @a dest_id.
     */
    void add_route (node_id_rep dest_id, route_info const & rinfo, route_order_enum order)
    {
        route r {rinfo, order};
        auto rindex = find_route(r);

        // Not found
        if (rindex < 0) {
            _routes.push_back(std::move(r));
            rindex = static_cast<int>(_routes.size() - 1);
        }

        // if (hops == 0) {
        //     add_sibling(id);
        //     return;
        // }

        // _routes.insert(std::make_pair(id, route_item{gwid, hops, true}));
    }

    /**
     * Searches gateway for destination node @a id.
     *
     * @details Preference is given to a route with a low value of hops and its reachability.
     */
    pfs::optional<node_id_rep> gateway_for (node_id_rep id)
    {
        // FIXME
        // // Check if direct access
        // if (_sibling_nodes.find(id) != _sibling_nodes.cend())
        //     return id;
        //
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

    /**
     * @param f Visitor function with signature:
     *          void (node_id_rep const & dest, node_id_rep const & gwid, unsigned int hops).
     *          If hops equals to zero it is a direct connection.
     */
    template <typename F>
    void foreach_route (F && f)
    {
        for (auto pos = _sibling_nodes.cbegin(); pos != _sibling_nodes.cend(); ++pos)
            f(*pos, *pos, 0);

        // FIXME
        // for (auto pos = _routes.cbegin(); pos != _routes.cend(); ++pos) {
        //     f(pos->second.gwid, pos->first, pos->second.hops);
        // }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Serialization methods
    ////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * Serialize initial request
     */
    std::vector<char> serialize_request (node_id_rep const & initiator_id)
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
    std::vector<char> serialize_request (node_id_rep const & gwid, route_info const & rinfo)
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
    std::vector<char> serialize_response (node_id_rep const & responder_id, route_info const & rinfo)
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

#if __COMMENT__
template <typename SerializerTraits>
class routing_table
{
public:
    using serializer_traits = SerializerTraits;

private:
    // N(0) --- N(gw0) --- N(gw1) --- N(1)
    // For N(0) gateway is N(gw0) and hops = 2 (number of intermediate gateways)
    struct route_item
    {
        node_id_rep gwid;         // Nearest gateway identifier
        unsigned int hops {0};    // 0 - direct access
        bool unreachable {false}; // True if node is unreachable by this route
    };

protected:
    node_id_rep _id; // Node ID representation

    // The first gateway is the default one
    std::vector<node_id_rep> _gateways;
    std::unordered_multimap<node_id_rep, route_item> _routes;
    std::unordered_set<node_id_rep> _sibling_nodes;

public:
    routing_table (node_id_rep const & id): _id(id) {}

    routing_table (routing_table const &) = delete;
    routing_table (routing_table &&) = delete;
    routing_table & operator = (routing_table const &) = delete;
    routing_table & operator = (routing_table &&) = delete;

    ~routing_table () = default;

public:
    std::size_t gateway_count () const noexcept
    {
        return _gateways.size();
    }

    void append_gateway (node_id_rep id)
    {
        // Check if already exists
        for (auto const & x: _gateways) {
            if (x == id)
                return;
        }

        _gateways.push_back(id);
    }

    void add_sibling (node_id_rep const & id)
    {
        _sibling_nodes.insert(id);
    }

    /**
     * Adds new route @a gwid to the destination node @a id.
     */
    void add_route (node_id_rep id, node_id_rep gwid, unsigned int hops)
    {
        if (hops == 0) {
            add_sibling(id);
            return;
        }

        _routes.insert(std::make_pair(id, route_item{gwid, hops, true}));
    }

    void remove_sibling (node_id_rep id)
    {
        _sibling_nodes.erase(id);
    }

    node_id_rep default_gateway () const
    {
        if (_gateways.empty())
            throw error {tr::_("no default gateway")};

        return _gateways[0];
    }

    // /**
    //  * Searches gateway for destination node @a id.
    //  *
    //  * @details Preference is given to a route with a low value of hops and its reachability.
    //  */
    // pfs::optional<node_id_rep> gateway_for (node_id_rep id)
    // {
    //     // Check if direct access
    //     if (_sibling_nodes.find(id) != _sibling_nodes.cend())
    //         return id;
    //
    //     auto hops = std::numeric_limits<unsigned int>::max();
    //     auto res = _routes.equal_range(id);
    //
    //     // If range not found return default gateway
    //     if (res.first == res.second)
    //         return default_gateway();
    //
    //     pfs::optional<node_id_rep> opt_gwid;
    //
    //     for (auto pos = res.first; pos != res.second; ++pos) {
    //         PFS__TERMINATE(pos->second.hops > 0, "Fix meshnet::routing_table algorithm");
    //
    //         if (hops > pos->second.hops && !pos->second.unreachable) {
    //             hops = pos->second.hops;
    //             opt_gwid = pos->second.gwid;
    //         }
    //     }
    //
    //     return opt_gwid;
    // }

    // FIXME REMOVE OR ?
    // void enabled_route (node_id_rep const & id, node_id_rep const & gwid, bool enabled)
    // {
    //     auto res = _routes.equal_range(id);
    //
    //     PFS__TERMINATE(res.first != res.second, "Fix meshnet::routing_table algorithm");
    //
    //     for (auto pos = res.first; pos != res.second; ++pos) {
    //         if (pos->second.gwid == gwid) {
    //             pos->second.unreachable = !enabled;
    //             break;
    //         }
    //     }
    // }

    // std::vector<char> serialize_request ()
    // {
    //     auto out = serializer_traits::make_serializer();
    //     route_packet pkt {packet_way_enum::request};
    //     pkt.rinfo.initiator_id = _id;
    //     pkt.serialize(out);
    //     return out.take();
    // }

    // std::vector<char> serialize_request (route_info const & rinfo)
    // {
    //     auto out = serializer_traits::make_serializer();
    //     route_packet pkt {packet_way_enum::request};
    //     pkt.rinfo = rinfo;
    //     pkt.rinfo.route.push_back(_id);
    //     pkt.serialize(out);
    //     return out.take();
    // }

    // std::vector<char> serialize_response (route_info const & rinfo, bool initial)
    // {
    //     auto out = serializer_traits::make_serializer();
    //     route_packet pkt {packet_way_enum::response};
    //     pkt.rinfo = rinfo;
    //
    //     if (initial)
    //         pkt.rinfo.responder_id = _id;
    //
    //     pkt.serialize(out);
    //     return out.take();
    // }
    //
    // template <typename F>
    // void foreach_gateway (F && f)
    // {
    //     for (auto const & gwid: _gateways)
    //         f(gwid);
    // }
};
#endif

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
