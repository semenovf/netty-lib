////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/callback.hpp"
#include "pfs/netty/poller_types.hpp"
#include "pfs/netty/patterns/priority_tracker.hpp"
#include "pfs/netty/patterns/delivery/manager.hpp"
#include "pfs/netty/patterns/delivery/delivery_controller.hpp"
#include "pfs/netty/patterns/meshnet/alive_controller.hpp"
#include "pfs/netty/patterns/meshnet/channel_map.hpp"
#include "pfs/netty/patterns/meshnet/dual_link_handshake.hpp"
#include "pfs/netty/patterns/meshnet/node.hpp"
#include "pfs/netty/patterns/meshnet/node_pool.hpp"
#include "pfs/netty/patterns/meshnet/node_pool_rd.hpp"
#include "pfs/netty/patterns/meshnet/infinite_reconnection_policy.hpp"
#include "pfs/netty/patterns/meshnet/input_controller.hpp"
#include "pfs/netty/patterns/meshnet/priority_writer_queue.hpp"
#include "pfs/netty/patterns/meshnet/routing_table.hpp"
#include "pfs/netty/patterns/meshnet/simple_heartbeat.hpp"
#include "pfs/netty/patterns/meshnet/simple_input_account.hpp"
#include "pfs/netty/patterns/meshnet/single_link_handshake.hpp"
#include "pfs/netty/patterns/meshnet/without_handshake.hpp"
#include "pfs/netty/patterns/meshnet/without_heartbeat.hpp"
#include "pfs/netty/patterns/meshnet/without_input_controller.hpp"
#include "pfs/netty/patterns/meshnet/without_reconnection_policy.hpp"
#include "pfs/netty/patterns/meshnet/writer_queue.hpp"
#include "pfs/netty/posix/tcp_listener.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"
#include "pfs/netty/traits/serializer_traits.hpp"
#include <pfs/fake_mutex.hpp>
#include <pfs/universal_id.hpp>
#include <pfs/universal_id_hash.hpp>
#include <pfs/universal_id_pack.hpp>

#if NETTY__QT_ENABLED
// #   include "pfs/netty/traits/qbytearray_archive_traits.hpp" // FIXME UNCOMMENT
#   include "../byte_array.hpp" // FIXME REMOVE
#else
// #   include "pfs/netty/traits/vector_archive_traits.hpp" // FIXME UNCOMMENT
#   include "../byte_array.hpp" // FIXME REMOVE
#endif

namespace delivery_ns = netty::patterns::delivery;
namespace meshnet_ns = netty::patterns::meshnet;

#if NETTY__QT_ENABLED
// using archive_type = QByteArray; // FIXME UNCOMMENT
using archive_type = byte_array;
#else
//using archive_type = std::vector<char>;  // FIXME UNCOMMENT
using archive_type = byte_array;
#endif

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

using serializer_traits_t = netty::serializer_traits<true, archive_type>;
using priority_tracker_t = netty::patterns::priority_tracker<priority_distribution>;
using node_id = pfs::universal_id;
using socket_id = netty::posix::tcp_socket::socket_id;

using writer_queue_t = meshnet_ns::writer_queue<archive_type>;
using priority_writer_queue_t = meshnet_ns::priority_writer_queue<priority_tracker_t, archive_type>;

using input_controller_t = meshnet_ns::input_controller<
      priority_tracker_t::SIZE
    , socket_id
    , node_id
    , serializer_traits_t>;

////////////////////////////////////////////////////////////////////////////////////////////////////
// node_t
////////////////////////////////////////////////////////////////////////////////////////////////////
using node_t = meshnet_ns::node<
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
    , serializer_traits_t
    , meshnet_ns::infinite_reconnection_policy
    , meshnet_ns::single_link_handshake // dual_link_handshake
    , meshnet_ns::simple_heartbeat
    , input_controller_t>;


////////////////////////////////////////////////////////////////////////////////////////////////////
// Node pool
////////////////////////////////////////////////////////////////////////////////////////////////////
// Choose required type here

using routing_table_t = meshnet_ns::routing_table<pfs::universal_id, serializer_traits_t>;
using alive_controller_t = meshnet_ns::alive_controller<pfs::universal_id
    , serializer_traits_t>;
using node_pool_t = meshnet_ns::node_pool<pfs::universal_id
    , routing_table_t
    , alive_controller_t
    , std::recursive_mutex>;

using message_id = pfs::universal_id;
using delivery_transport_t = node_pool_t;
using delivery_controller_t = delivery_ns::delivery_controller<node_id, message_id
    , serializer_traits_t, priority_tracker_t>;

using delivery_manager_t = delivery_ns::manager<delivery_transport_t, message_id
    , delivery_controller_t, std::recursive_mutex>;

using reliable_node_pool_t = meshnet_ns::node_pool_rd<delivery_manager_t>;
