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

#if NETTY__SELECT_IMPL
    using delivery_engine = netty::p2p::delivery_engine<netty::p2p::select_engine_traits>;
#elif NETTY__POLL_IMPL
    using delivery_engine = netty::p2p::delivery_engine<netty::p2p::poll_engine_traits>;
#elif NETTY__EPOLL_IMPL
    using delivery_engine = netty::p2p::delivery_engine<netty::p2p::epoll_engine_traits>;
#elif NETTY__ENET_IMPL
    using delivery_engine = netty::p2p::delivery_engine<netty::p2p::enet_engine_traits>;
#elif NETTY__UDT_IMPL
    using delivery_engine = netty::p2p::delivery_engine<netty::p2p::udt_engine_traits>;
#else
#   error "No implementation defined"
#endif

using reliable_delivery_engine = netty::p2p::reliable_delivery_engine<delivery_engine, netty::sample::persistent_storage>;
using serializer = reliable_delivery_engine::serializer_type;

//#if NETTY__SELECT_ENABLED
//using select_delivery_engine = netty::p2p::delivery_engine<netty::p2p::select_engine_traits>;
//using select_reliable_delivery_engine = netty::p2p::reliable_delivery_engine<select_delivery_engine, netty::sample::persistent_storage>;
//using select_serializer = select_reliable_delivery_engine::serializer_type;
//#endif
//
//#if NETTY__EPOLL_ENABLED
//using epoll_delivery_engine = netty::p2p::delivery_engine<netty::p2p::epoll_engine_traits>;
//using epoll_reliable_delivery_engine = netty::p2p::reliable_delivery_engine<epoll_delivery_engine, netty::sample::persistent_storage>;
//using epoll_serializer = epoll_reliable_delivery_engine::serializer_type;
//#endif
//
//#if NETTY__POLL_ENABLED
//using poll_delivery_engine = netty::p2p::delivery_engine<netty::p2p::poll_engine_traits>;
//using poll_reliable_delivery_engine = netty::p2p::reliable_delivery_engine<poll_delivery_engine, netty::sample::persistent_storage>;
//using poll_serializer = poll_reliable_delivery_engine::serializer_type;
//#endif
//
//#if NETTY__ENET_ENABLED
//using enet_delivery_engine = netty::p2p::delivery_engine<netty::p2p::enet_engine_traits>;
//using enet_reliable_delivery_engine = netty::p2p::reliable_delivery_engine<enet_delivery_engine, netty::sample::persistent_storage>;
//using enet_serializer = enet_reliable_delivery_engine::serializer_type;
//#endif
//
//#if NETTY__UDT_ENABLED
//using udt_delivery_engine = netty::p2p::delivery_engine<netty::p2p::udt_engine_traits>;
//using udt_reliable_delivery_engine = netty::p2p::reliable_delivery_engine<udt_delivery_engine, netty::sample::persistent_storage>;
//using udt_serializer = udt_reliable_delivery_engine::serializer_type;
//#endif
