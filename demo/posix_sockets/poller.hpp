////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/poller.hpp"

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
using client_epoll_poller_type = netty::client_poller<netty::linux_ns::epoll_poller>;
using server_epoll_poller_type = netty::server_poller<netty::linux_ns::epoll_poller>;
#endif
