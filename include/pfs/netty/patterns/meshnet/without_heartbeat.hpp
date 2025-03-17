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
#include "../../socket4_addr.hpp"

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class without_heartbeat
{
    using socket_id = typename Node::socket_id;
    using serializer_traits = typename Node::serializer_traits;

public:
    without_heartbeat (Node &) {}

public:
    void update (socket_id) {}
    void remove (socket_id) {}
    void process (socket_id, heartbeat_packet const &) {}
    unsigned int step () { return 0; }

    template <typename F>
    without_heartbeat & on_expired (F &&)
    {
        return *this;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
