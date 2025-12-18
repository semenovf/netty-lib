////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.27 Initial version (`node_index.hpp`).
//      2025.12.18 Renamed to `peer_index.hpp`.
//                 `peer_index_t` renamed to `peer_index_t`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <cstdint>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

// Peer index (started from 1)
using peer_index_t = std::uint16_t;
constexpr peer_index_t INVALID_PEER_INDEX = peer_index_t{0};

} // namespace meshnet

NETTY__NAMESPACE_END
