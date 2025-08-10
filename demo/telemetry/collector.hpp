////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.07.29 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#if TELEMETRY_ZMQ_MSGPACK_BACKEND

#   include "msgpack_serializer.hpp"
#   include "zmq_subscriber.hpp"
#   include "pfs/netty/telemetry/collector.hpp"

using collector_t = netty::telemetry::collector<zmq_subscriber, msgpack_deserializer, std::string>;

#elif TELEMETRY_QT_BACKEND

#   include "qt_serializer.hpp"
#   include "qt_subscriber.hpp"
#   include "pfs/netty/telemetry/collector.hpp"

using collector_t = netty::telemetry::collector<qt_subscriber, qt_deserializer, std::string>;

#else

#   include <pfs/netty/poller_types.hpp>
#   include <pfs/netty/posix/tcp_socket.hpp>
#   include "pfs/netty/telemetry/collector.hpp"
#   include "pfs/netty/telemetry/serializer.hpp"
#   include "pfs/netty/patterns/pubsub/subscriber.hpp"

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
>;

using deserializer_t = netty::telemetry::deserializer;
using collector_t = netty::telemetry::collector<subscriber_t, deserializer_t, std::string>;

#endif // TELEMETRY_ZMQ_MSGPACK_BACKEND
