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
#include "../../socket4_addr.hpp"
#include "handshake_result.hpp"
#include <string>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class without_handshake
{
    using socket_id = typename Node::socket_id;
    using node_id_rep = typename Node::node_id_rep;
    using channel_collection_type = typename Node::channel_collection_type;

public:
    without_handshake (Node *, channel_collection_type *) {}

public:
    mutable callback_t<void (socket_id)> on_expired;
    mutable callback_t<void (node_id_rep, socket_id, std::string const &, bool
        , handshake_result_enum)> on_completed;

    void start (socket_id, bool) {}
    void cancel (socket_id) {}
    unsigned int step () { return 0; }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
