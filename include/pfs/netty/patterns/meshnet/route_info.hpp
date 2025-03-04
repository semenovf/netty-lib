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
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

