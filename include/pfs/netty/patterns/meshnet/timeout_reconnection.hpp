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
#include <chrono>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class timeout_reconnection
{
    Node & _node;
    std::chrono::seconds _timeout {5};

public:
    timeout_reconnection (Node & node)
        : _node(node)
    {}

public:
    void configure (std::chrono::seconds timeout)
    {
        if (timeout < std::chrono::seconds{0})
            timeout = std::chrono::seconds{0};

        if (timeout > std::chrono::seconds{3600 * 24})
            timeout = std::chrono::seconds{3600 * 24};

        _timeout = timeout;
    }

    void operator () (socket4_addr const & saddr)
    {
        _node._connecting_pool.connect_timeout(_timeout, saddr);
    }

    void operator () (typename Node::socket_id sid)
    {
        bool is_accepted = false;
        auto psock = _node._socket_pool.locate(sid, & is_accepted);
        auto reconnecting = !is_accepted;

        if (reconnecting)
            _node._connecting_pool.connect_timeout(_timeout, psock->saddr());

        _node._socket_pool.close(sid);
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
