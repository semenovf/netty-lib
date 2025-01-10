////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.19 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "connecting_poller.hpp"
#include "listener_poller.hpp"
#include "reader_poller.hpp"
#include "writer_poller.hpp"

#if NETTY__SELECT_ENABLED
#   include "posix/select_poller.hpp"
NETTY__NAMESPACE_BEGIN
using connecting_select_poller_t = connecting_poller<posix::select_poller>;
using listener_select_poller_t = listener_poller<posix::select_poller>;
using reader_select_poller_t = reader_poller<posix::select_poller>;
using writer_select_poller_t = writer_poller<posix::select_poller>;
NETTY__NAMESPACE_END
#endif

#if NETTY__POLL_ENABLED
#   include "posix/poll_poller.hpp"
NETTY__NAMESPACE_BEGIN
using connecting_poll_poller_t = connecting_poller<posix::poll_poller>;
using listener_poll_poller_t = listener_poller<posix::poll_poller>;
using reader_poll_poller_t = reader_poller<posix::poll_poller>;
using writer_poll_poller_t = writer_poller<posix::poll_poller>;
NETTY__NAMESPACE_END
#endif

#if NETTY__EPOLL_ENABLED
#   include "linux/epoll_poller.hpp"
NETTY__NAMESPACE_BEGIN
using connecting_epoll_poller_t = connecting_poller<linux_os::epoll_poller>;
using listener_epoll_poller_t = listener_poller<linux_os::epoll_poller>;
using reader_epoll_poller_t = reader_poller<linux_os::epoll_poller>;
using writer_epoll_poller_t = writer_poller<linux_os::epoll_poller>;
NETTY__NAMESPACE_END
#endif

#if NETTY__UDT_ENABLED
#   include "udt/epoll_poller.hpp"
NETTY__NAMESPACE_BEGIN
using connecting_udt_poller_t = connecting_poller<netty::udt::epoll_poller>;
using listener_udt_poller_t = listener_poller<netty::udt::epoll_poller>;
using reader_udt_poller_t = reader_poller<linux_os::epoll_poller>;
using writer_udt_poller_t = writer_poller<linux_os::epoll_poller>;
NETTY__NAMESPACE_END
#endif

#if NETTY__ENET_ENABLED
#   include "enet/enet_poller.hpp"
NETTY__NAMESPACE_BEGIN
using connecting_enet_poller_t = connecting_poller<netty::enet::enet_poller>;
using listener_enet_poller_t = listener_poller<netty::enet::enet_poller>;
using reader_enet_poller_t = reader_poller<netty::enet::enet_poller>;
using writer_enet_poller_t = writer_poller<netty::enet::enet_poller>;
NETTY__NAMESPACE_END
#endif

