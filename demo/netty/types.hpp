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
#include "pfs/netty/posix/tcp_server.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"
#include "pfs/netty/udt/udt_server.hpp"
#include "pfs/netty/udt/udt_socket.hpp"

using tcp_socket_type = netty::posix::tcp_socket;
using tcp_server_type = netty::posix::tcp_server;
using udt_socket_type = netty::udt::udt_socket<>;
using udt_server_type = netty::udt::udt_server<>;
