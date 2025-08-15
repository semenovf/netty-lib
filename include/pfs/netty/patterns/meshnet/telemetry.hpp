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
#include "telemetry_keys.hpp"
#include <cstdint>
#include <memory>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

using telemetry_producer_t = telemetry::producer_u16_t;
using telemetry_consumer_t = telemetry::consumer_u16_t;
using telemetry_visitor_interface_t = telemetry::visitor_interface_u16_t;
using shared_telemetry_producer_t = std::shared_ptr<telemetry_producer_t>;

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

