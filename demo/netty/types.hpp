////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/server_poller.hpp"
#include "pfs/netty/client_poller.hpp"
#include "pfs/netty/posix/tcp_server.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"
#include "pfs/netty/udt/udt_server.hpp"
#include "pfs/netty/udt/udt_socket.hpp"

using tcp_socket_type = netty::posix::tcp_socket;
using tcp_server_type = netty::posix::tcp_server;
using udt_socket_type = netty::udt::udt_socket<>;
using udt_server_type = netty::udt::udt_server<>;

#if NETTY__SELECT_ENABLED
#   include "pfs/netty/posix/select_poller.hpp"
using client_select_poller_type = netty::client_poller<netty::posix::select_poller>;
using server_select_poller_type = netty::server_poller<netty::posix::select_poller>;
#endif

#if NETTY__POLL_ENABLED
#   include "pfs/netty/posix/poll_poller.hpp"
using client_poll_poller_type = netty::client_poller<netty::posix::poll_poller>;
using server_poll_poller_type = netty::server_poller<netty::posix::poll_poller>;
#endif

#if NETTY__EPOLL_ENABLED
#   include "pfs/netty/linux/epoll_poller.hpp"
using client_epoll_poller_type = netty::client_poller<netty::linux::epoll_poller>;
using server_epoll_poller_type = netty::server_poller<netty::linux::epoll_poller>;
#endif

#if NETTY__UDT_ENABLED
#   include "pfs/netty/udt/epoll_poller.hpp"
using client_udt_poller_type = netty::client_poller<netty::udt::epoll_poller>;
using server_udt_poller_type = netty::server_poller<netty::udt::epoll_poller>;
#endif
