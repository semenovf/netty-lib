////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.12.30 Initial version.
//      2023.01.19 Initial version v2.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/bits/operationsystem.h"
#include "pfs/netty/poller_types.hpp"
#include "pfs/netty/posix/tcp_server.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"

#if NETTY__P2P_UDT_ENGINE
#   include "pfs/netty/udt/udt_server.hpp"
#   include "pfs/netty/udt/udt_socket.hpp"
#endif

namespace netty {
namespace p2p {

struct default_engine_traits
{

#if NETTY__P2P_UDT_ENGINE

    using client_poller_type = netty::client_udt_poller_type;
    using server_poller_type = netty::server_udt_poller_type;
    using reader_type = netty::udt::udt_socket;
    using writer_type = netty::udt::udt_socket;
    using server_type = netty::udt::udt_server;

#elif NETTY__P2P_EPOLL_ENGINE

    using client_poller_type = netty::client_epoll_poller_type;
    using server_poller_type = netty::server_epoll_poller_type;
    using reader_type = netty::posix::tcp_socket;
    using writer_type = netty::posix::tcp_socket;
    using server_type = netty::posix::tcp_server;

#elif NETTY__P2P_POLL_ENGINE

    using client_poller_type = netty::client_epoll_poller_type;
    using server_poller_type = netty::server_epoll_poller_type;
    using reader_type = netty::posix::tcp_socket;
    using writer_type = netty::posix::tcp_socket;
    using server_type = netty::posix::tcp_server;

#elif NETTY__P2P_SELECT_ENGINE

    using client_poller_type = netty::client_select_poller_type;
    using server_poller_type = netty::server_select_poller_type;
    using reader_type = netty::posix::tcp_socket;
    using writer_type = netty::posix::tcp_socket;
    using server_type = netty::posix::tcp_server;

#else // NETTY__P2P_UDT_ENGINE

#   if PFS_OS_LINUX

    using client_poller_type = netty::client_epoll_poller_type;
    using server_poller_type = netty::server_epoll_poller_type;
    using reader_type = netty::posix::tcp_socket;
    using writer_type = netty::posix::tcp_socket;
    using server_type = netty::posix::tcp_server;

#   elif PFS_OS_WIN

    using client_poller_type = netty::client_select_poller_type;
    using server_poller_type = netty::server_select_poller_type;
    using reader_type = netty::posix::tcp_socket;
    using writer_type = netty::posix::tcp_socket;
    using server_type = netty::posix::tcp_server;

#   elif PFS_OS_ANDROID

    using client_poller_type = netty::client_poll_poller_type;
    using server_poller_type = netty::server_poll_poller_type;
    using reader_type = netty::posix::tcp_socket;
    using writer_type = netty::posix::tcp_socket;
    using server_type = netty::posix::tcp_server;

#   else
#       error "Unsupported platform"
#   endif

#endif // !NETTY__P2P_UDT_ENGINE

    // They are usually identical
    using writer_id = typename client_poller_type::native_socket_type;
    using reader_id = typename server_poller_type::native_socket_type;
};

}} // namespace netty::p2p
