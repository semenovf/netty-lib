////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.19 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "server_poller.hpp"
#include "client_poller.hpp"

#if NETTY__SELECT_ENABLED
#   include "posix/select_poller.hpp"
namespace netty {
using client_select_poller_type = client_poller<posix::select_poller>;
using server_select_poller_type = server_poller<posix::select_poller>;
} // namespace netty
#endif

#if NETTY__POLL_ENABLED
#   include "posix/poll_poller.hpp"
namespace netty {
using client_poll_poller_type = client_poller<posix::poll_poller>;
using server_poll_poller_type = server_poller<posix::poll_poller>;
} // namespace netty
#endif

#if NETTY__EPOLL_ENABLED
#   include "linux/epoll_poller.hpp"
namespace netty {
using client_epoll_poller_type = client_poller<linux::epoll_poller>;
using server_epoll_poller_type = server_poller<linux::epoll_poller>;
} // namespace netty
#endif

#if NETTY__UDT_ENABLED
#   include "udt/epoll_poller.hpp"
namespace netty {
using client_udt_poller_type = netty::client_poller<netty::udt::epoll_poller>;
using server_udt_poller_type = netty::server_poller<netty::udt::epoll_poller>;
} // namespace netty
#endif
