////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.12.30 Initial version.
//      2023.01.19 Initial version v2.
//      2024.07.22 Replaced default_engine_traits by concrete engine traits.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/poller_types.hpp"
#include "pfs/netty/posix/tcp_listener.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"

#if NETTY__UDT_ENABLED
#   include "pfs/netty/udt/udt_server.hpp"
#   include "pfs/netty/udt/udt_socket.hpp"
#endif

#if NETTY__ENET_ENABLED
#   include "pfs/netty/enet/enet_listener.hpp"
#   include "pfs/netty/enet/enet_socket.hpp"
#endif

namespace netty {
namespace p2p {

#if defined(NETTY__SELECT_ENABLED)
struct select_engine_traits
{
    using client_poller_type = netty::client_select_poller_type;
    using server_poller_type = netty::server_select_poller_type;
    using reader_type   = netty::posix::tcp_socket;
    using writer_type   = netty::posix::tcp_socket;
    using listener_type = netty::posix::tcp_listener;

    // DEPRECATED
    using writer_id_type = typename client_poller_type::native_socket_type;
    using reader_id_type = typename server_poller_type::native_socket_type;
};
#endif

#if defined(NETTY__POLL_ENABLED)
struct poll_engine_traits
{
    using client_poller_type = netty::client_poll_poller_type;
    using server_poller_type = netty::server_poll_poller_type;
    using reader_type   = netty::posix::tcp_socket;
    using writer_type   = netty::posix::tcp_socket;
    using listener_type = netty::posix::tcp_listener;

    // DEPRECATED
    using writer_id_type = typename client_poller_type::native_socket_type;
    using reader_id_type = typename server_poller_type::native_socket_type;
};
#endif

#if defined(NETTY__EPOLL_ENABLED)
struct epoll_engine_traits
{
    using client_poller_type = netty::client_epoll_poller_type;
    using server_poller_type = netty::server_epoll_poller_type;
    using reader_type   = netty::posix::tcp_socket;
    using writer_type   = netty::posix::tcp_socket;
    using listener_type = netty::posix::tcp_listener;

    // DEPRECATED
    using writer_id_type = typename client_poller_type::native_socket_type;
    using reader_id_type = typename server_poller_type::native_socket_type;
};
#endif

#if NETTY__UDT_ENABLED
struct udt_engine_traits
{
    using client_poller_type = netty::client_udt_poller_type;
    using server_poller_type = netty::server_udt_poller_type;
    using reader_type   = netty::udt::udt_socket;
    using writer_type   = netty::udt::udt_socket;
    using listener_type = netty::udt::udt_server;
};
#endif

#if NETTY__ENET_ENABLED
struct enet_engine_traits
{
    using client_poller_type = netty::client_enet_poller_type;
    using server_poller_type = netty::server_enet_poller_type;
    using reader_type   = netty::enet::enet_socket;
    using writer_type   = netty::enet::enet_socket;
    using listener_type = netty::enet::enet_listener;
};
#endif

}} // namespace netty::p2p
