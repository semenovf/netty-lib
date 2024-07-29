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
#   include "pfs/netty/udt/udt_listener.hpp"
#endif

#if NETTY__ENET_ENABLED
#   include "pfs/netty/enet/enet_listener.hpp"
#   include "pfs/netty/enet/enet_socket.hpp"
#endif

struct select_server_traits
{
    using listener_type = netty::posix::tcp_listener;
    using socket_type   = netty::posix::tcp_socket;
    using poller_type   = netty::server_select_poller_type;
    using native_socket_type = poller_type::native_socket_type;
};

struct poll_server_traits
{
    using listener_type = netty::posix::tcp_listener;
    using socket_type   = netty::posix::tcp_socket;
    using poller_type   = netty::server_poll_poller_type;
    using native_socket_type = poller_type::native_socket_type;
};

struct epoll_server_traits
{
    using listener_type = netty::posix::tcp_listener;
    using socket_type   = netty::posix::tcp_socket;
    using poller_type   = netty::server_epoll_poller_type;
    using native_socket_type = poller_type::native_socket_type;
};

struct select_client_traits
{
    using socket_type   = netty::posix::tcp_socket;
    using poller_type   = netty::client_select_poller_type;
    using native_socket_type = poller_type::native_socket_type;
};

struct poll_client_traits
{
    using socket_type   = netty::posix::tcp_socket;
    using poller_type   = netty::client_poll_poller_type;
    using native_socket_type = poller_type::native_socket_type;
};

struct epoll_client_traits
{
    using socket_type   = netty::posix::tcp_socket;
    using poller_type   = netty::client_epoll_poller_type;
    using native_socket_type = poller_type::native_socket_type;
};

#if NETTY__UDT_ENABLED

struct udt_server_traits
{
    using listener_type = netty::udt::udt_listener;
    using socket_type   = netty::udt::udt_socket;
    using poller_type   = netty::server_udt_poller_type;
    using native_socket_type = poller_type::native_socket_type;
};

struct udt_client_traits
{
    using socket_type   = netty::udt::udt_socket;
    using poller_type   = netty::client_udt_poller_type;
    using native_socket_type = poller_type::native_socket_type;
};

#endif

#if NETTY__ENET_ENABLED

struct enet_server_traits
{
    using listener_type = netty::enet::enet_listener;
    using socket_type   = netty::enet::enet_socket;
    using poller_type   = netty::server_enet_poller_type;
    using native_socket_type = poller_type::native_socket_type;
};

struct enet_client_traits
{
    using socket_type   = netty::enet::enet_socket;
    using poller_type   = netty::client_enet_poller_type;
    using native_socket_type = poller_type::native_socket_type;
};

#endif
