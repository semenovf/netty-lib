////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.07.30 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <string>

#if TELEMETRY_ZMQ_MSGPACK_BACKEND

#   include "msgpack_serializer.hpp"
#   include "zmq_publisher.hpp"
#   include "pfs/netty/telemetry/producer.hpp"
#   include <memory>

using producer_t = netty::telemetry::producer<zmq_publisher, msgpack_serializer, std::string>;

#elif TELEMETRY_QT_BACKEND

#   include "qt_serializer.hpp"
#   include "qt_publisher.hpp"
#   include "pfs/netty/telemetry/producer.hpp"
#   include <memory>

using producer_t = netty::telemetry::producer<qt_publisher, qt_serializer, std::string>;

#else

#   include <pfs/netty/poller_types.hpp>
#   include <pfs/netty/posix/tcp_listener.hpp>
#   include <pfs/netty/posix/tcp_socket.hpp>
#   include "pfs/netty/telemetry/producer.hpp"
#   include "pfs/netty/telemetry/serializer.hpp"
#   include "pfs/netty/patterns/pubsub/publisher.hpp"
#   include "pfs/netty/patterns/pubsub/writer_queue.hpp"

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

using serializer_t = netty::telemetry::serializer;
using producer_t = netty::telemetry::producer<publisher_t, serializer_t, std::string>;

#endif // TELEMETRY_ZMQ_MSGPACK_BACKEND
