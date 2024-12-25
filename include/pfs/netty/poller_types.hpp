////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.19 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "listener_poller.hpp"
#include "server_poller.hpp"
#include "client_poller.hpp"

#if NETTY__SELECT_ENABLED
#   include "posix/select_poller.hpp"
namespace netty {
using listener_select_poller_t = listener_poller<posix::select_poller>;
using client_select_poller_t = client_poller<posix::select_poller>;
using server_select_poller_t = server_poller<posix::select_poller>;
} // namespace netty
#endif

#if NETTY__POLL_ENABLED
#   include "posix/poll_poller.hpp"
namespace netty {
using listener_poll_poller_t = listener_poller<posix::poll_poller>;
using client_poll_poller_t = client_poller<posix::poll_poller>;
using server_poll_poller_t = server_poller<posix::poll_poller>;
} // namespace netty
#endif

#if NETTY__EPOLL_ENABLED
#   include "linux/epoll_poller.hpp"
namespace netty {
using listener_epoll_poller_t = listener_poller<linux_os::epoll_poller>;
using client_epoll_poller_t = client_poller<linux_os::epoll_poller>;
using server_epoll_poller_t = server_poller<linux_os::epoll_poller>;
} // namespace netty
#endif

#if NETTY__UDT_ENABLED
#   include "udt/epoll_poller.hpp"
namespace netty {
using listener_udt_poller_t = listener_poller<netty::udt::epoll_poller>;
using client_udt_poller_t = client_poller<netty::udt::epoll_poller>;
using server_udt_poller_t = server_poller<netty::udt::epoll_poller>;
} // namespace netty
#endif

#if NETTY__ENET_ENABLED
#   include "enet/enet_poller.hpp"
namespace netty {
using listener_enet_poller_t = listener_poller<netty::enet::epoll_poller>;
using client_enet_poller_t = client_poller<netty::enet::enet_poller>;
using server_enet_poller_t = server_poller<netty::enet::enet_poller>;
} // namespace netty
#endif

