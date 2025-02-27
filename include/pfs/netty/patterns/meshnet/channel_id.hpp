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

namespace patterns {
namespace meshnet {

// Channel ID started from 1
using channel_id = std::uint16_t;
constexpr channel_id INVALID_CHANNEL_ID = channel_id{0};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
