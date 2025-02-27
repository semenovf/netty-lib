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
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeId>
struct functional_channel_callbacks
{
    // Notify when connection established with the remote node
    std::function<void (NodeId)> on_channel_established = [] (NodeId) {};

    // Notify when the channel is destroyed with the remote node
    std::function<void (NodeId)> on_channel_destroyed = [] (NodeId) {};

    // Notify when data actually sent (written into the socket)
    std::function<void (NodeId, std::uint64_t)> on_bytes_written
        = [] (NodeId, std::uint64_t /*n*/) {};

    // On intermediate route info received
    std::function<void (NodeId, bool, std::vector<std::pair<std::uint64_t, std::uint64_t>> &&)> on_route_received
        = [] (NodeId, bool /*is_response*/, std::vector<std::pair<std::uint64_t, std::uint64_t>> && /*route*/) {};

    // On domestic message received
    std::function<void (NodeId, int, std::vector<char> &&)> on_message_received
        = [] (NodeId, int /*priority*/, std::vector<char> && /*bytes*/) {};

    // On foreign (intersubnet) message received
    std::function<void (NodeId
        , int
        , std::pair<std::uint64_t, std::uint64_t>
        , std::pair<std::uint64_t, std::uint64_t>
        , std::vector<char> &&)> on_foreign_message_received
        = [] (NodeId
            , int /*priority*/
            , std::pair<std::uint64_t, std::uint64_t> /*sender_id*/
            , std::pair<std::uint64_t, std::uint64_t> /*receiver_id*/
            , std::vector<char> && /*bytes*/) {};
};

template <typename NodeId>
struct functional_node_callbacks
{
    // Notify when connection established with the remote node
    std::function<void (NodeId)> on_channel_established = [] (NodeId) {};

    // Notify when the channel is destroyed with the remote node
    std::function<void (NodeId)> on_channel_destroyed = [] (NodeId) {};

    // Notify when data actually sent (written into the socket)
    std::function<void (NodeId, std::uint64_t)> on_bytes_written
        = [] (NodeId, std::uint64_t /*n*/) {};

    // On domestic message received
    std::function<void (NodeId, int, std::vector<char> &&)> on_message_received
        = [] (NodeId, int /*priority*/, std::vector<char> && /*bytes*/) {};
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

