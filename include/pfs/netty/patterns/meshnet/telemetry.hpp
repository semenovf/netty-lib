////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../telemetry/telemetry.hpp"
#include <cstdint>
#include <memory>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

using telemetry_producer_t = telemetry::producer_u16_t;
using shared_telemetry_producer_t = std::shared_ptr<telemetry_producer_t>;

constexpr telemetry_producer_t::key_type KEY_INITIAL = 1000;

// When produced: after channel established between two nodes.
// Type: string.
// Format: JSON array [Node0_ID, Node1_ID], where Node0_ID is the sender.
constexpr telemetry_producer_t::key_type KEY_CHANNEL_ESTABLISHED = 1 + KEY_INITIAL;

// When produced: after channel destroyed between two nodes.
// Type: string.
// Format: JSON array [Node0_ID, Node1_ID], where Node0_ID is the sender.
constexpr telemetry_producer_t::key_type KEY_CHANNEL_DESTROYED = 2 + KEY_INITIAL;

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

