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
#include <pfs/optional.hpp>
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

template <typename NodeId>
struct route_info
{
    NodeId initiator_id;
    NodeId responder_id; // not used when request

    std::vector<NodeId> route; // Gateways chain

public:
    /**
     * Find gateway index in the route.
     */
    pfs::optional<std::size_t> gateway_index (NodeId gw_id) const
    {
        for (std::size_t i = 0; i < route.size(); i++) {
            if (gw_id == route[i])
                return i;
        }

        return pfs::nullopt;
    }
};

} // namespace meshnet

NETTY__NAMESPACE_END

