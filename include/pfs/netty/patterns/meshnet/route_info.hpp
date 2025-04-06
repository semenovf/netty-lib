////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "node_id_rep.hpp"
#include <pfs/optional.hpp>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

struct route_info
{
    node_id_rep initiator_id;
    node_id_rep responder_id; // not used when request

    std::vector<node_id_rep> route; // router IDs

public:
    /**
     * Find gateway index in the route.
     */
    pfs::optional<std::size_t> gateway_index (node_id_rep gw) const
    {
        for (std::size_t i = 0; i < route.size(); i++) {
            if (gw == route[i])
                return i;
        }

        return pfs::nullopt;
    }

    std::vector<node_id_rep> reverse_route () const
    {
        std::vector<node_id_rep> reversed_route(route.size());
        std::reverse_copy(route.begin(), route.end(), reversed_route.begin());
        return reversed_route;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

