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
#include <pfs/netty/socket4_addr.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class without_reconnection
{
public:
    without_reconnection (Node &) {}

public:
    void operator () (socket4_addr const & saddr) {}
    void operator () (typename Node::socket_id) {}
    void configure () {}
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

