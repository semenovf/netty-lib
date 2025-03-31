////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.25 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../socket4_addr.hpp"

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class without_handshake
{
    using socket_id = typename Node::socket_id;
    using channel_collection_type = typename Node::channel_collection_type;

public:
    without_handshake (Node *, channel_collection_type *) {}

public:
    template <typename F> without_handshake & on_expired (F &&) { return *this; }
    template <typename F> without_handshake & on_completed (F &&)  { return *this; }

    void start (socket_id, bool) {}
    void cancel (socket_id) {}
    unsigned int step () { return 0; }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
