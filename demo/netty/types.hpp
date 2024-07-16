////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/poller_types.hpp"
#include "pfs/netty/posix/tcp_listener.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"

#if NETTY__UDT_ENABLED
#   include "pfs/netty/udt/udt_socket.hpp"
#   include "pfs/netty/udt/udt_server.hpp"
#endif

#if NETTY__ENET_ENABLED
#   include "pfs/netty/enet/enet_listener.hpp"
#   include "pfs/netty/enet/enet_socket.hpp"
#endif

using tcp_listener_type = netty::posix::tcp_listener;
using tcp_socket_type   = netty::posix::tcp_socket;

#if NETTY__UDT_ENABLED
using udt_listener_type = netty::udt::udt_server<>;
using udt_socket_type   = netty::udt::udt_socket<>;
#endif

#if NETTY__ENET_ENABLED
using enet_listener_type = netty::enet::enet_listener;
using enet_socket_type = netty::enet::enet_socket;
#endif
