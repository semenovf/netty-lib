////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/namespace.hpp>
#include <functional>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
struct functional_callbacks
{
    // Notify when connection established with the remote node
    std::function<void(typename Node::node_id)> on_node_connected = [] (typename Node::node_id) {};

    // Notify when connection disconnected with the remote node
    std::function<void(typename Node::node_id)> on_node_disconnected = [] (typename Node::node_id) {};

    // On data/message received
    std::function<void(typename Node::node_id, std::vector<char> &&)> on_message_received
        = [] (typename Node::node_id, std::vector<char> && bytes) {};
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

