////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../pubsub/suitable_pubsub.hpp"
#include "../telemetry/consumer.hpp"
#include "../telemetry/producer.hpp"
#include "telemetry_keys.hpp"
#include <cstdint>
#include <memory>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

template <typename SerializerTraits>
using telemetry_producer = telemetry::producer<std::uint16_t
    , netty::pubsub::suitable_publisher<SerializerTraits>>;

template <typename SerializerTraits>
using telemetry_consumer = telemetry::consumer<std::uint16_t
    , netty::pubsub::suitable_subscriber<SerializerTraits>>;

} // namespace meshnet

NETTY__NAMESPACE_END
