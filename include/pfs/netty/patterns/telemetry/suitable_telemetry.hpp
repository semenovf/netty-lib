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
#include "../pubsub/suitable_pubsub.hpp"
#include "consumer.hpp"
#include "producer.hpp"
#include "serializer.hpp"
#include "visitor.hpp"
#include <cstdint>
#include <string>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace telemetry {

template <typename Archive = std::vector<char>>
using suitable_consumer = consumer<std::string, netty::patterns::pubsub::suitable_subscriber<Archive>, deserializer<std::string>>;

template <typename Archive = std::vector<char>>
using suitable_producer = producer<std::string, netty::patterns::pubsub::suitable_publisher<Archive>, serializer<std::string>>;

using visitor_interface_t  = visitor_interface<std::string>;

template <typename Archive = std::vector<char>>
using suitable_consumer_u8 = consumer<std::uint8_t, netty::patterns::pubsub::suitable_subscriber<Archive>, deserializer<std::uint8_t>>;

template <typename Archive = std::vector<char>>
using suitable_producer_u8 = producer<std::uint8_t, netty::patterns::pubsub::suitable_publisher<Archive>, serializer<std::uint8_t>>;

using visitor_interface_u8_t = visitor_interface<std::uint8_t>;

template <typename Archive = std::vector<char>>
using suitable_consumer_u16 = consumer<std::uint16_t, netty::patterns::pubsub::suitable_subscriber<Archive>, deserializer<std::uint16_t>>;

template <typename Archive = std::vector<char>>
using suitable_producer_u16 = producer<std::uint16_t, netty::patterns::pubsub::suitable_publisher<Archive>, serializer<std::uint16_t>>;

using visitor_interface_u16_t = visitor_interface<std::uint16_t>;

template <typename Archive = std::vector<char>>
using suitable_consumer_u32 = consumer<std::uint32_t, netty::patterns::pubsub::suitable_subscriber<Archive>, deserializer<std::uint32_t>>;

template <typename Archive = std::vector<char>>
using suitable_producer_u32 = producer<std::uint32_t, netty::patterns::pubsub::suitable_publisher<Archive>, serializer<std::uint32_t>>;

using visitor_interface_u32_t = visitor_interface<std::uint32_t>;

template <typename Archive = std::vector<char>>
using suitable_consumer_u64 = consumer<std::uint64_t, netty::patterns::pubsub::suitable_subscriber<Archive>, deserializer<std::uint64_t>>;

template <typename Archive = std::vector<char>>
using suitable_producer_u64 = producer<std::uint64_t, netty::patterns::pubsub::suitable_publisher<Archive>, serializer<std::uint64_t>>;

using visitor_interface_u64_t = visitor_interface<std::uint64_t>;

}} // namespace patterns::telemetry

NETTY__NAMESPACE_END
