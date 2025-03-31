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
#include <cstdint>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

struct route_info
{
    std::uint64_t utctime {0}; // Send time point in milliseconds since epoch in UTC
    node_id_rep initiator_id;
    node_id_rep responder_id; // not used when request

    std::vector<node_id_rep> route; // router IDs

public:
    /**
     * Find gateway index in the route.
     */
    pfs::optional<std::size_t> gateway_index (node_id_rep const & id) const
    {
        for (std::size_t i = 0; i < route.size(); i++) {
            if (id == route[i])
                return i;
        }

        return pfs::nullopt;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

