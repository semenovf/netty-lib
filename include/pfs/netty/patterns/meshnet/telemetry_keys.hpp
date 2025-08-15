////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.14 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cstdint>

using telemetry_key_t = std::uint16_t;

constexpr telemetry_key_t KEY_INITIAL = 1000;

// When produced: after channel established between two nodes.
// Type: string.
// Format: JSON array [Node0_ID, Node1_ID], where Node0_ID is the sender.
constexpr telemetry_key_t KEY_CHANNEL_ESTABLISHED = 1 + KEY_INITIAL;

// When produced: after channel destroyed between two nodes.
// Type: string.
// Format: JSON array [Node0_ID, Node1_ID], where Node0_ID is the sender.
constexpr telemetry_key_t KEY_CHANNEL_DESTROYED = 2 + KEY_INITIAL;
