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

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace telemetry {

using consumer_t = consumer<netty::patterns::pubsub::subscriber_t, deserializer>;
using producer_t = producer<netty::patterns::pubsub::publisher_t, serializer>;

}} // namespace patterns::telemetry

NETTY__NAMESPACE_END
