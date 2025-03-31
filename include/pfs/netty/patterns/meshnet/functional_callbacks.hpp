////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "alive_info.hpp"
#include "node_index.hpp"
#include "route_info.hpp"
#include <pfs/log.hpp>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

struct node_callbacks
{
    std::function<void (std::string const &)> on_error = [] (std::string const & msg) { LOGE("[node]", "{}", msg); };

    // Notify when connection established with the remote node
    std::function<void (node_id_rep const &, node_index_t, bool)> on_channel_established = [] (node_id_rep const &, node_index_t, bool /*is_gateway*/) {};

    // Notify when the channel is destroyed with the remote node
    std::function<void (node_id_rep const &, node_index_t)> on_channel_destroyed = [] (node_id_rep const &, node_index_t) {};

    // Notify when data actually sent (written into the socket)
    std::function<void (node_id_rep const &, std::uint64_t)> on_bytes_written
        = [] (node_id_rep const &, std::uint64_t /*n*/) {};

    // On alive info received
    std::function<void (node_id_rep const &, node_index_t, alive_info const &)> on_alive_received
        = [] (node_id_rep const &, node_index_t, alive_info const &) {};

    // On unreachable node received
    std::function<void (node_id_rep const &, node_index_t, unreachable_info const &)> on_unreachable_received
        = [] (node_id_rep const &, node_index_t, unreachable_info const &) {};

    // On intermediate route info received
    std::function<void (node_id_rep const &, node_index_t, bool, route_info const &)> on_route_received
        = [] (node_id_rep const &, node_index_t, bool /*is_response*/, route_info const &) {};

    // On domestic message received
    std::function<void (node_id_rep const &, int, std::vector<char> &&)> on_domestic_message_received
        = [] (node_id_rep const &, int /*priority*/, std::vector<char> && /*bytes*/) {};

    // On global (intersubnet) message received
    std::function<void (node_id_rep const & // last transmitter node
        , int // priority
        , node_id_rep const & // sender ID
        , node_id_rep const & // receiver ID
        , std::vector<char> &&)> on_global_message_received
        = [] (node_id_rep const &, int /*priority*/, node_id_rep const & /*sender_id*/
            , node_id_rep const & /*receiver_id*/, std::vector<char> && /*bytes*/) {};

    // On global (intersubnet) message received
    std::function<void (int // priority
        , node_id_rep const & // sender ID
        , node_id_rep const & // receiver ID
        , std::vector<char> &&)> forward_global_message
        = [] (int /*priority*/, node_id_rep const & /*sender_id*/, node_id_rep const & /*receiver_id*/
            , std::vector<char> && /*packet*/) {};
};

struct node_pool_callbacks
{
    std::function<void (std::string const &)> on_error = [] (std::string const & msg) { LOGE("[node_pool]", "{}", msg); };

    // Notify when connection established with the remote node
    std::function<void (node_id_rep const &, bool)> on_channel_established = [] (node_id_rep const &, bool /*is_gateway*/) {};

    // Notify when the channel is destroyed with the remote node
    std::function<void (node_id_rep const &)> on_channel_destroyed = [] (node_id_rep const &) {};

    // Notify when node alive status changed
    std::function<void (node_id_rep const &)> on_node_alive = [] (node_id_rep const &) {};

    // Notify when node alive status changed
    std::function<void (node_id_rep const &)> on_node_expired = [] (node_id_rep const &) {};

    // Notify when data actually sent (written into the socket)
    std::function<void (node_id_rep const &, std::uint64_t)> on_bytes_written
        = [] (node_id_rep const &, std::uint64_t /*n*/) {};

    // Notify when message received (domestic or global)
    std::function<void (node_id_rep const &, int, std::vector<char> &&)> on_message_received
        = [] (node_id_rep const &, int /*priority*/, std::vector<char> && /*bytes*/) {};
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
