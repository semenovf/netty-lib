////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/fake_mutex.hpp>
#include <pfs/universal_id.hpp>
#include <pfs/universal_id_hash.hpp>
#include <pfs/universal_id_pack.hpp>
#include <pfs/netty/callback.hpp>
#include <pfs/netty/poller_types.hpp>
#include <pfs/netty/patterns/serializer_traits.hpp>
#include <pfs/netty/patterns/delivery/incoming_controller.hpp>
#include <pfs/netty/patterns/delivery/manager.hpp>
#include <pfs/netty/patterns/delivery/outgoing_controller.hpp>
#include <pfs/netty/patterns/delivery/session_manager.hpp>
#include <pfs/netty/patterns/meshnet/alive_controller.hpp>
#include <pfs/netty/patterns/meshnet/channel_map.hpp>
#include <pfs/netty/patterns/meshnet/dual_link_handshake.hpp>
#include <pfs/netty/patterns/meshnet/node.hpp>
#include <pfs/netty/patterns/meshnet/node_pool.hpp>
#include <pfs/netty/patterns/meshnet/node_pool_rd.hpp>
#include <pfs/netty/patterns/meshnet/priority_input_controller.hpp>
#include <pfs/netty/patterns/meshnet/priority_writer_queue.hpp>
#include <pfs/netty/patterns/meshnet/reconnection_policy.hpp>
#include <pfs/netty/patterns/meshnet/routing_table.hpp>
#include <pfs/netty/patterns/meshnet/simple_heartbeat.hpp>
#include <pfs/netty/patterns/meshnet/simple_input_controller.hpp>
#include <pfs/netty/patterns/meshnet/single_link_handshake.hpp>
#include <pfs/netty/patterns/meshnet/without_handshake.hpp>
#include <pfs/netty/patterns/meshnet/without_heartbeat.hpp>
#include <pfs/netty/patterns/meshnet/without_input_controller.hpp>
#include <pfs/netty/patterns/meshnet/without_reconnection_policy.hpp>
#include <pfs/netty/writer_queue.hpp>
#include <pfs/netty/posix/tcp_listener.hpp>
#include <pfs/netty/posix/tcp_socket.hpp>

namespace delivery_ns = netty::patterns::delivery;
namespace meshnet_ns = netty::patterns::meshnet;

using node_id = pfs::universal_id;
using priority_writer_queue_t = meshnet_ns::priority_writer_queue<3>;

template <typename Node>
using priority_input_controller = meshnet_ns::priority_input_controller<3, Node>;

////////////////////////////////////////////////////////////////////////////////////////////////////
// nopriority_meshnet_node_t
////////////////////////////////////////////////////////////////////////////////////////////////////
using nopriority_meshnet_node_t = meshnet_ns::node<
      node_id
    , netty::posix::tcp_socket
    , netty::posix::tcp_listener

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
    , netty::writer_queue
    , pfs::fake_mutex
    , netty::patterns::serializer_traits_t
    , meshnet_ns::reconnection_policy
    , meshnet_ns::single_link_handshake // dual_link_handshake
    , meshnet_ns::simple_heartbeat
    , meshnet_ns::simple_input_controller>;

////////////////////////////////////////////////////////////////////////////////////////////////////
// priority_meshnet_node_t
////////////////////////////////////////////////////////////////////////////////////////////////////
using priority_meshnet_node_t = meshnet_ns::node<
      node_id
    , netty::posix::tcp_socket
    , netty::posix::tcp_listener

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
    , priority_writer_queue_t
    , pfs::fake_mutex
    , netty::patterns::serializer_traits_t
    , meshnet_ns::reconnection_policy
    , meshnet_ns::single_link_handshake // dual_link_handshake
    , meshnet_ns::simple_heartbeat
    , priority_input_controller>;

////////////////////////////////////////////////////////////////////////////////////////////////////
// bare_meshnet_node_t
////////////////////////////////////////////////////////////////////////////////////////////////////
// Unusable, for test without_XXX parameters only.
using bare_meshnet_node_t = meshnet_ns::node<
      node_id
    , netty::posix::tcp_socket
    , netty::posix::tcp_listener

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
    , netty::writer_queue
    , pfs::fake_mutex
    , netty::patterns::serializer_traits_t
    , meshnet_ns::without_reconnection_policy
    , meshnet_ns::without_handshake
    , meshnet_ns::without_heartbeat
    , meshnet_ns::without_input_controller>;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Node pool
////////////////////////////////////////////////////////////////////////////////////////////////////
// Choose required type here
//using node_t = nopriority_meshnet_node_t;
using node_t = priority_meshnet_node_t;

using routing_table_t = meshnet_ns::routing_table<pfs::universal_id, netty::patterns::serializer_traits_t>;
using alive_controller_t = meshnet_ns::alive_controller<pfs::universal_id
    , netty::patterns::serializer_traits_t>;
using node_pool_t = meshnet_ns::node_pool<pfs::universal_id
    , routing_table_t
    , alive_controller_t
    , std::recursive_mutex>;

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Reliable delivery node pool
////////////////////////////////////////////////////////////////////////////////////////////////////
struct priority_distribution
{
    std::array<std::size_t, 3> distrib {9, 3, 1};

    static constexpr std::size_t SIZE = 3;

    constexpr std::size_t operator [] (std::size_t i) const noexcept
    {
        return distrib[i];
    }
};

using priority_tracker_t = netty::patterns::priority_tracker<priority_distribution>;

using delivery_transport_t = node_pool_t;
using incoming_controller_t = delivery_ns::incoming_controller<pfs::universal_id
    , netty::patterns::serializer_traits_t, priority_tracker_t::SIZE>;
using outgoing_controller_t = delivery_ns::outgoing_controller<pfs::universal_id
    , netty::patterns::serializer_traits_t, priority_tracker_t>;

using delivery_manager_t = delivery_ns::manager<delivery_transport_t, pfs::universal_id
    , incoming_controller_t, outgoing_controller_t, std::recursive_mutex>;
using delivery_session_manager_t = delivery_ns::session_manager;

using reliable_node_pool_t = meshnet_ns::node_pool_rd<delivery_manager_t, delivery_session_manager_t>;
using message_id = reliable_node_pool_t::message_id;
