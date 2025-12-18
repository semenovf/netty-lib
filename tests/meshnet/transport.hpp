////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../serializer_traits.hpp"
#include "pfs/netty/callback.hpp"
#include "pfs/netty/poller_types.hpp"
#include "pfs/netty/patterns/priority_tracker.hpp"
#include "pfs/netty/patterns/delivery/manager.hpp"
#include "pfs/netty/patterns/delivery/delivery_controller.hpp"
#include "pfs/netty/patterns/meshnet/channel_map.hpp"
#include "pfs/netty/patterns/meshnet/dual_link_handshake.hpp"
#include "pfs/netty/patterns/meshnet/peer.hpp"
#include "pfs/netty/patterns/meshnet/node.hpp"
#include "pfs/netty/patterns/meshnet/reliable_node.hpp"
#include "pfs/netty/patterns/meshnet/infinite_reconnection_policy.hpp"
#include "pfs/netty/patterns/meshnet/input_controller.hpp"
#include "pfs/netty/patterns/meshnet/priority_writer_queue.hpp"
#include "pfs/netty/patterns/meshnet/routing_table.hpp"
#include "pfs/netty/patterns/meshnet/heartbeat_controller.hpp"
#include "pfs/netty/patterns/meshnet/single_link_handshake.hpp"
#include "pfs/netty/posix/tcp_listener.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"
#include <pfs/fake_mutex.hpp>
#include <pfs/universal_id.hpp>
#include <pfs/universal_id_hash.hpp>
#include <pfs/universal_id_pack.hpp>

namespace delivery_ns = netty::delivery;
namespace meshnet_ns = netty::meshnet;

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Reliable delivery node pool
////////////////////////////////////////////////////////////////////////////////////////////////////
struct priority_distribution
{
    std::array<std::size_t, 3> distrib {5, 3, 1};

    static constexpr std::size_t SIZE = 3;

    constexpr std::size_t operator [] (std::size_t i) const noexcept
    {
        return distrib[i];
    }
};

using priority_tracker_t = netty::priority_tracker<priority_distribution>;
using node_id = pfs::universal_id;
using socket_id = netty::posix::tcp_socket::socket_id;

using priority_writer_queue_t = meshnet_ns::priority_writer_queue<priority_tracker_t, serializer_traits_t>;
using input_controller_t = meshnet_ns::input_controller<
      priority_tracker_t::SIZE
    , socket_id
    , node_id
    , serializer_traits_t>;

using handshake_controller_t = meshnet_ns::single_link_handshake<socket_id, node_id, serializer_traits_t>;
using heartbeat_controller_t = meshnet_ns::heartbeat_controller<socket_id, serializer_traits_t>;
using reconnection_policy_t  = meshnet_ns::infinite_reconnection_policy;

////////////////////////////////////////////////////////////////////////////////////////////////////
// peer_t
////////////////////////////////////////////////////////////////////////////////////////////////////
using peer_t = meshnet_ns::peer<
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
    , reconnection_policy_t
    , handshake_controller_t
    , heartbeat_controller_t
    , input_controller_t>;


////////////////////////////////////////////////////////////////////////////////////////////////////
// Node pool
////////////////////////////////////////////////////////////////////////////////////////////////////
// Choose required type here

using routing_table_t = meshnet_ns::routing_table<pfs::universal_id, serializer_traits_t>;

using unreliable_node_t = meshnet_ns::node<pfs::universal_id
    , routing_table_t
    , std::recursive_mutex>;

#if NETTY__TESTS_USE_MESHNET_NODE_POOL_RD
using message_id = pfs::universal_id;
using delivery_transport_t = unreliable_node_t;
using delivery_controller_t = delivery_ns::delivery_controller<node_id, message_id
    , serializer_traits_t, priority_tracker_t>;

using delivery_manager_t = delivery_ns::manager<delivery_transport_t, message_id
    , delivery_controller_t, std::recursive_mutex>;

using reliable_node_t = meshnet_ns::reliable_node<delivery_manager_t>;
using node_t = reliable_node_t;
#else
using node_t = unreliable_node_t;
#endif

