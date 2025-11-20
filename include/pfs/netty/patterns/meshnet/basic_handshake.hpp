////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.30 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../callback.hpp"
#include "protocol.hpp"
#include <pfs/assert.hpp>
#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class basic_handshake
{
protected:
    using node_id = typename Node::node_id;
    using socket_id = typename Node::socket_id;
    using serializer_traits_type = typename Node::serializer_traits_type;
    using serializer_type = typename serializer_traits_type::serializer_type;
    using archive_type = typename serializer_traits_type::archive_type;
    using time_point_type = std::chrono::steady_clock::time_point;

protected:
    Node * _node {nullptr};
    std::map<socket_id, time_point_type> _cache;
    std::chrono::seconds _timeout {3};

public:
    mutable callback_t<void (socket_id)> on_expired;
    mutable callback_t<void (socket_id, archive_type)> enqueue_packet;
    mutable callback_t<void (node_id, socket_id /*reader_sid*/, socket_id /*writer_sid*/
        , bool /*is_gateway*/)> on_completed;
    mutable callback_t<void (node_id, socket_id /*sid*/, bool /*force_closing*/)> on_duplicate_id;
    mutable callback_t<void (node_id, socket_id /*sid*/)> on_discarded;

protected:
    basic_handshake (Node * node)
        : _node(node)
    {}

protected:
    void enqueue_request (socket_id sid, bool behind_nat)
    {
        archive_type ar;
        serializer_type out {ar};
        handshake_packet<node_id> pkt {_node->is_gateway(), behind_nat, packet_way_enum::request};

        pkt.id = _node->id();
        pkt.serialize(out);

        // Cache socket ID as handshake initiator
        _cache[sid] = std::chrono::steady_clock::now() + _timeout;

        enqueue_packet(sid, std::move(ar));
    }

    void enqueue_response (socket_id sid, bool behind_nat)
    {
        archive_type ar;
        serializer_type out {ar};
        handshake_packet<node_id> pkt {_node->is_gateway(), behind_nat, packet_way_enum::response};

        pkt.id = _node->id();
        pkt.serialize(out);

        enqueue_packet(sid, std::move(ar));
    }

    unsigned int check_expired ()
    {
        unsigned int result = 0;

        if (!_cache.empty()) {
            auto now = std::chrono::steady_clock::now();

            for (auto pos = _cache.begin(); pos != _cache.end();) {
                if (pos->second <= now) { // Time limit exceeded
                    auto sid = pos->first;
                    pos = _cache.erase(pos);
                    result++;
                    this->on_expired(sid);
                } else {
                    ++pos;
                }
            }
        }

        return result;
    }

public:
    void start (socket_id sid, bool behind_nat)
    {
        enqueue_request(sid, behind_nat);
    }

    bool cancel (socket_id sid)
    {
        auto erased = _cache.erase(sid) > 0;
        return erased;
    }

    unsigned int step ()
    {
        return check_expired();
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
