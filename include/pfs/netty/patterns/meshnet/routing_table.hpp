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
#include <pfs/optional.hpp>
#include <limits>
#include <vector>
#include <unordered_map>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdTraits, typename SerializerTraits>
class routing_table
{
public:
    using node_id = typename NodeIdTraits::node_id;
    using serializer_traits = SerializerTraits;

private:
    // N(0) --- N(gw0) --- N(gw1) --- N(1)
    // For N(0) gateway is N(gw0) and hops = 2 (number of intermediate gateways)
    struct route_item
    {
        node_id gwid;          // for direct access no matter the gateway
        unsigned int hops {0}; // 0 - direct access
    };

protected:
    std::pair<std::uint64_t, std::uint64_t> _id_pair; // Node ID representation

    // The first gateway is the default one
    std::vector<node_id> _gateways;
    std::unordered_multimap<node_id, route_item> _routes;

public:
    routing_table (node_id id)
    {
        _id_pair.first = NodeIdTraits::high(id);
        _id_pair.second = NodeIdTraits::low(id);
    }

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

    void append_gateway (node_id id)
    {
        // Check if already exists
        for (auto const & x: _gateways) {
            if (x == id)
                return;
        }

        _gateways.push_back(id);
    }

    void add_route (node_id id, node_id gw, unsigned int hops)
    {
        _routes.insert(std::make_pair(id, route_item{gw, hops}));
    }

    node_id default_gateway () const
    {
        if (_gateways.empty())
            throw error {tr::_("no default gateway")};

        return _gateways[0];
    }

    node_id gateway_for (node_id id)
    {
        auto hops = std::numeric_limits<unsigned int>::max();
        auto res = _routes.equal_range(id);

        // If range not found return default gateway
        if (res.first == res.second)
            return default_gateway();

        node_id gwid{};

        for (auto pos = res.first; pos != res.second; ++pos) {
            // Direct access
            if (pos->second.hops == 0) {
                gwid = id;
                break;
            }

            if (hops > pos->second.hops) {
                hops = pos->second.hops;
                gwid = pos->second.gwid;
            }
        }

        return gwid;
    }

    std::vector<char> serialize_request ()
    {
        auto out = serializer_traits::make_serializer();
        route_packet pkt {packet_way_enum::request};
        pkt.rinfo.initiator_id = _id_pair;
        pkt.serialize(out);
        return out.take();
    }

    std::vector<char> serialize_request (route_info const & rinfo)
    {
        auto out = serializer_traits::make_serializer();
        route_packet pkt {packet_way_enum::request};
        pkt.rinfo = rinfo;
        pkt.rinfo.route.push_back(_id_pair);
        pkt.serialize(out);
        return out.take();
    }

    std::vector<char> serialize_response (route_info const & rinfo, bool initial)
    {
        auto out = serializer_traits::make_serializer();
        route_packet pkt {packet_way_enum::response};
        pkt.rinfo = rinfo;

        if (initial)
            pkt.rinfo.responder_id = _id_pair;

        pkt.serialize(out);
        return out.take();
    }

    template <typename F>
    void foreach_gateway (F && f)
    {
        for (auto const & gwid: _gateways)
            f(gwid);
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
