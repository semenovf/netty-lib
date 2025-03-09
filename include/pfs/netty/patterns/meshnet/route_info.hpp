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
#include <cstdint>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

struct route_info
{
    std::uint64_t utctime {0}; // Send time point in milliseconds since epoch in UTC
    std::pair<std::uint64_t, std::uint64_t> initiator_id;
    std::pair<std::uint64_t, std::uint64_t> responder_id; // not used when request

    // first - high part, second - low part
    std::vector<std::pair<std::uint64_t, std::uint64_t>> route; // router IDs

public:
    /**
     * Find gateway index in the route.
     */
    pfs::optional<std::size_t> gateway_index (std::pair<std::uint64_t, std::uint64_t> const & id) const
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

