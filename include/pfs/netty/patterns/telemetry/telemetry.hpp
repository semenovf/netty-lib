////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../pubsub/pubsub.hpp"
#include "consumer.hpp"
#include "producer.hpp"
#include "serializer.hpp"
#include <cstdint>
#include <string>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace telemetry {

using consumer_t = consumer<std::string, netty::patterns::pubsub::subscriber_t, deserializer<std::string>>;
using producer_t = producer<std::string, netty::patterns::pubsub::publisher_t, serializer<std::string>>;

using consumer_u8_t = consumer<std::uint8_t, netty::patterns::pubsub::subscriber_t, deserializer<std::uint8_t>>;
using producer_u8_t = producer<std::uint8_t, netty::patterns::pubsub::publisher_t, serializer<std::uint8_t>>;

using consumer_u16_t = consumer<std::uint16_t, netty::patterns::pubsub::subscriber_t, deserializer<std::uint16_t>>;
using producer_u16_t = producer<std::uint16_t, netty::patterns::pubsub::publisher_t, serializer<std::uint16_t>>;

using consumer_u32_t = consumer<std::uint32_t, netty::patterns::pubsub::subscriber_t, deserializer<std::uint32_t>>;
using producer_u32_t = producer<std::uint32_t, netty::patterns::pubsub::publisher_t, serializer<std::uint32_t>>;

using consumer_u64_t = consumer<std::uint64_t, netty::patterns::pubsub::subscriber_t, deserializer<std::uint64_t>>;
using producer_u64_t = producer<std::uint64_t, netty::patterns::pubsub::publisher_t, serializer<std::uint64_t>>;

}} // namespace patterns::telemetry

NETTY__NAMESPACE_END
