////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025-2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.04 Initial version.
//      2026.07.16 Added session_id `sid` field to `route_info` struct.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "session_id.hpp"
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
    session_id_t session_id;
    NodeId initiator_id;
    NodeId responder_id; // Used by response only
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

