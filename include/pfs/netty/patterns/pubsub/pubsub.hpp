////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "pfs/netty/poller_types.hpp"
#include "pfs/netty/posix/tcp_listener.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"
#include "pfs/netty/patterns/pubsub/input_controller.hpp"
#include "pfs/netty/patterns/pubsub/publisher.hpp"
#include "pfs/netty/patterns/pubsub/subscriber.hpp"
#include "pfs/netty/patterns/pubsub/writer_queue.hpp"

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace pubsub {

using publisher_t = netty::patterns::pubsub::publisher<
      netty::posix::tcp_socket
    , netty::posix::tcp_listener
#if NETTY__EPOLL_ENABLED
    , netty::listener_epoll_poller_t
    , netty::writer_epoll_poller_t
#elif NETTY__POLL_ENABLED
    , netty::listener_poll_poller_t
    , netty::writer_poll_poller_t
#elif NETTY__SELECT_ENABLED
    , netty::listener_select_poller_t
    , netty::writer_select_poller_t
#endif
    , netty::patterns::pubsub::writer_queue
    , std::recursive_mutex>;

using subscriber_t = netty::patterns::pubsub::subscriber<
      netty::posix::tcp_socket
#if NETTY__EPOLL_ENABLED
    , netty::connecting_epoll_poller_t
    , netty::reader_epoll_poller_t
#elif NETTY__POLL_ENABLED
    , netty::connecting_poll_poller_t
    , netty::reader_poll_poller_t
#elif NETTY__SELECT_ENABLED
    , netty::connecting_select_poller_t
    , netty::reader_select_poller_t
#endif
    , netty::patterns::pubsub::input_controller>;

}} // namespace patterns::pubsub

NETTY__NAMESPACE_END
