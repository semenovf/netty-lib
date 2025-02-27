////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/patterns/console_logger.hpp>
#include <pfs/netty/patterns/reliable_delivery/engine.hpp>
#include <pfs/netty/patterns/reliable_delivery/protocol.hpp>
#include <pfs/netty/patterns/reliable_delivery/simple_protocol_traits.hpp>

using delivery_engine_t = struct {};

using reliable_delivery_engine_t = netty::patterns::reliable_delivery::engine<
      delivery_engine_t
    , netty::patterns::reliable_delivery::simple_protocol_traits
    , netty::patterns::console_logger>;
