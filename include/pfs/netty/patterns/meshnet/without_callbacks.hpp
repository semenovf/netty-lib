////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/namespace.hpp>
#include <string>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
struct without_callbacks
{
    static void on_node_ready (typename Node::node_id) {}
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END


