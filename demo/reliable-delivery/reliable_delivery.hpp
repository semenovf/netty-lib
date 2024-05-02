////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "persistent_storage.hpp"
#include "netty/p2p/delivery_engine.hpp"
#include "netty/p2p/discovery_engine.hpp"
#include "netty/p2p/primal_serializer.hpp"
#include "netty/p2p/reliable_delivery_engine.hpp"
#include "netty/p2p/posix/discovery_engine.hpp"

using discovery_engine = netty::p2p::discovery_engine<netty::p2p::posix::discovery_engine>;
using delivery_engine = netty::p2p::delivery_engine<>;
using reliable_delivery_engine = netty::p2p::reliable_delivery_engine<delivery_engine, persistent_storage>;
using serializer = reliable_delivery_engine::serializer_type;
