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
#include "../telemetry/suitable_telemetry.hpp"
#include "telemetry_keys.hpp"
#include <cstdint>
#include <memory>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

using telemetry_producer_t = telemetry::suitable_producer_u16<std::vector<char>>;
using telemetry_consumer_t = telemetry::suitable_consumer_u16<std::vector<char>>;
using telemetry_visitor_interface_t = telemetry::visitor_interface_u16_t;
using shared_telemetry_producer_t = std::shared_ptr<telemetry_producer_t>;

} // namespace meshnet

NETTY__NAMESPACE_END
