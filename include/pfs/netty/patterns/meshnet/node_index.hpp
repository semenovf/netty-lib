////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <cstdint>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

// Node index (started from 1)
using node_index_t = std::uint16_t;
constexpr node_index_t INVALID_NODE_INDEX = node_index_t{0};

} // namespace meshnet

NETTY__NAMESPACE_END
