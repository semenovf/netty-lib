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
#include "../../callback.hpp"
#include <string>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class without_handshake
{
    using node_id = typename Node::node_id;
    using socket_id = typename Node::socket_id;

public:
    without_handshake (Node *) {}

public:
    mutable callback_t<void (socket_id)> on_expired;
    mutable callback_t<void (socket_id, std::vector<char> &&)> enqueue_packet;
    mutable callback_t<void (node_id, socket_id /*reader_sid*/, socket_id /*writer_sid*/
        , bool /*is_gateway*/)> on_completed;
    mutable callback_t<void (node_id, socket_id /*sid*/, bool /*force_closing*/)> on_duplicate_id;
    mutable callback_t<void (node_id, socket_id /*sid*/)> on_discarded;

    void start (socket_id, bool) {}
    void cancel (socket_id) {}
    unsigned int step () { return 0; }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
