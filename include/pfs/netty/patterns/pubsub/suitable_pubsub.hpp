////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../serializer_traits.hpp"
#include "../../poller_types.hpp"
#include "../../posix/tcp_listener.hpp"
#include "../../posix/tcp_socket.hpp"
#include "input_controller.hpp"
#include "publisher.hpp"
#include "subscriber.hpp"
#include "writer_queue.hpp"
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace pubsub {

/**
 * Suitable (cross-platform) publisher for the current platform.
 */
template <typename SerializerTraits = serializer_traits<>>
using suitable_publisher = netty::pubsub::publisher<
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
    , netty::pubsub::writer_queue<SerializerTraits>
    , std::recursive_mutex>;

/**
 * Suitable (cross-platform) subscriber for the current platform.
 */
template <typename SerializerTraits = serializer_traits<>>
using suitable_subscriber = subscriber<
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
    , input_controller<SerializerTraits>>;

} // namespace pubsub

NETTY__NAMESPACE_END
