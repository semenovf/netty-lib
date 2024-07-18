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

struct select_server_poller
{
    using listener_type = netty::posix::tcp_listener;
    using socket_type   = netty::posix::tcp_socket;
    using poller_type   = netty::server_select_poller_type;
    using native_socket_type = poller_type::native_socket_type;

    static poller_type create_poller (std::function<native_socket_type(native_socket_type, bool &)> && accept_proc)
    {
        return poller_type{std::move(accept_proc)};
    }
};

struct poll_server_poller
{
    using listener_type = netty::posix::tcp_listener;
    using socket_type   = netty::posix::tcp_socket;
    using poller_type   = netty::server_poll_poller_type;
    using native_socket_type = poller_type::native_socket_type;

    static poller_type create_poller (std::function<native_socket_type(native_socket_type, bool &)> && accept_proc)
    {
        return poller_type{std::move(accept_proc)};
    }
};

struct epoll_server_poller
{
    using listener_type = netty::posix::tcp_listener;
    using socket_type   = netty::posix::tcp_socket;
    using poller_type   = netty::server_epoll_poller_type;
    using native_socket_type = poller_type::native_socket_type;

    static poller_type create_poller (std::function<native_socket_type(native_socket_type, bool &)> && accept_proc)
    {
        return poller_type{std::move(accept_proc)};
    }
};

struct select_client_poller
{
    using socket_type   = netty::posix::tcp_socket;
    using poller_type   = netty::client_select_poller_type;
    using native_socket_type = poller_type::native_socket_type;

    static poller_type create_poller ()
    {
        return poller_type{};
    }
};

struct poll_client_poller
{
    using socket_type   = netty::posix::tcp_socket;
    using poller_type   = netty::client_poll_poller_type;
    using native_socket_type = poller_type::native_socket_type;

    static poller_type create_poller ()
    {
        return poller_type{};
    }
};

struct epoll_client_poller
{
    using socket_type   = netty::posix::tcp_socket;
    using poller_type   = netty::client_epoll_poller_type;
    using native_socket_type = poller_type::native_socket_type;

    static poller_type create_poller ()
    {
        return poller_type{};
    }
};

#if NETTY__UDT_ENABLED

struct udt_server_poller
{
    using listener_type = netty::udt::udt_server<>;
    using socket_type   = netty::udt::udt_socket<>;
    using poller_type   = netty::server_udt_poller_type;
    using native_socket_type = poller_type::native_socket_type;

    static poller_type create_poller (std::function<native_socket_type(native_socket_type, bool &)> && accept_proc)
    {
        return poller_type{std::move(accept_proc)};
    }
};

struct udt_client_poller
{
    using socket_type   = netty::udt::udt_socket<>;
    using poller_type   = netty::client_udt_poller_type;
    using native_socket_type = poller_type::native_socket_type;

    static poller_type create_poller ()
    {
        return poller_type{};
    }
};

#endif

#if NETTY__ENET_ENABLED

struct enet_server_poller
{
    using listener_type = netty::enet::enet_listener;
    using socket_type   = netty::enet::enet_socket;
    using poller_type   = netty::server_enet_poller_type;
    using native_socket_type = poller_type::native_socket_type;

    static poller_type create_poller (std::function<native_socket_type(native_socket_type, bool &)> && accept_proc)
    {
        return poller_type{std::make_shared<netty::enet::enet_poller>(), std::move(accept_proc)};
    }
};

struct enet_client_poller
{
    using socket_type   = netty::enet::enet_socket;
    using poller_type   = netty::client_enet_poller_type;
    using native_socket_type = poller_type::native_socket_type;

    static poller_type create_poller ()
    {
        return poller_type{std::make_shared<netty::enet::enet_poller>()};
    }
};

#endif
