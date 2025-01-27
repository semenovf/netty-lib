////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/patterns/meshnet/node.hpp>
#include <pfs/netty/patterns/meshnet/basic_input_processor.hpp>
#include <pfs/netty/patterns/meshnet/console_logger.hpp>
#include <pfs/netty/patterns/meshnet/exclusive_handshake.hpp>
#include <pfs/netty/patterns/meshnet/functional_callbacks.hpp>
#include <pfs/netty/patterns/meshnet/reconnection_policy.hpp>
#include <pfs/netty/patterns/meshnet/serializer_traits.hpp>
#include <pfs/netty/patterns/meshnet/simple_heartbeat.hpp>
#include <pfs/netty/patterns/meshnet/universal_id_traits.hpp>
#include <pfs/netty/patterns/meshnet/without_handshake.hpp>
#include <pfs/netty/patterns/meshnet/without_heartbeat.hpp>
#include <pfs/netty/poller_types.hpp>
#include <pfs/netty/priority_writer_queue.hpp>
#include <pfs/netty/writer_queue.hpp>
#include <pfs/netty/posix/tcp_listener.hpp>
#include <pfs/netty/posix/tcp_socket.hpp>

using meshnet_node_t = netty::patterns::meshnet::node<
      netty::patterns::meshnet::universal_id_traits
    , netty::posix::tcp_listener
    , netty::posix::tcp_socket

#if NETTY__EPOLL_ENABLED
    , netty::connecting_epoll_poller_t
    , netty::listener_epoll_poller_t
    , netty::reader_epoll_poller_t
    , netty::writer_epoll_poller_t
#elif NETTY__POLL_ENABLED
    , netty::connecting_poll_poller_t
    , netty::listener_poll_poller_t
    , netty::reader_poll_poller_t
    , netty::writer_poll_poller_t
#elif NETTY__SELECT_ENABLED
    , netty::connecting_select_poller_t
    , netty::listener_select_poller_t
    , netty::reader_select_poller_t
    , netty::writer_select_poller_t
#endif
    , netty::writer_queue // netty::priority_writer_queue<3>
    , netty::patterns::meshnet::default_serializer_traits_t
    , netty::patterns::meshnet::reconnection_policy
    , netty::patterns::meshnet::exclusive_handshake
    , netty::patterns::meshnet::simple_heartbeat
    , netty::patterns::meshnet::basic_input_processor
    , netty::patterns::meshnet::functional_callbacks
    , netty::patterns::meshnet::console_logger>;
