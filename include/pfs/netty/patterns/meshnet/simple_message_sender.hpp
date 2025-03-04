////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "protocol.hpp"
#include "../../namespace.hpp"
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class simple_message_sender
{
    using socket_id = typename Node::socket_id;
    using serializer_traits = typename Node::serializer_traits;

private:
    Node * _node {nullptr};

public:
    simple_message_sender (Node & node)
        : _node(& node)
    {}

public:
    void enqueue (socket_id sid, int priority, bool has_checksum, char const * data, std::size_t len)
    {
        auto out = serializer_traits::make_serializer();
        ddata_packet pkt {has_checksum};
        pkt.serialize(out, data, len);
        _node->enqueue_private(sid, priority, out.data(), out.size());
    }

    void enqueue (socket_id sid, int priority, bool has_checksum, std::vector<char> && data)
    {
        auto out = serializer_traits::make_serializer();
        ddata_packet pkt {has_checksum};
        pkt.serialize(out, std::move(data));
        _node->enqueue_private(sid, priority, out.data(), out.size());
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
