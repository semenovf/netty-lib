////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.30 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "handshake_result.hpp"
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
    using node_id_rep = typename Node::node_id_rep;
    using socket_id = typename Node::socket_id;
    using serializer_traits = typename Node::serializer_traits;
    using time_point_type = std::chrono::steady_clock::time_point;
    using channel_collection_type = typename Node::channel_collection_type;

protected:
    Node * _node {nullptr};
    channel_collection_type * _channels {nullptr};
    std::map<socket_id, time_point_type> _cache;
    std::chrono::seconds _timeout {3};

    mutable std::function<void(socket_id)> _on_expired;
    mutable std::function<void(node_id_rep, socket_id, std::string const & /*name*/
        , bool /*is_gateway*/, handshake_result_enum res)> _on_completed;

protected:
    basic_handshake (Node * node, channel_collection_type * channels)
        : _node(node)
        , _channels(channels)
    {}

protected:
    void enqueue_request (socket_id sid, bool behind_nat)
    {
        auto out = serializer_traits::make_serializer();
        handshake_packet pkt {_node->is_gateway(), behind_nat};

        pkt.id_rep = _node->id_rep();
        pkt.name = _node->name();
        pkt.serialize(out);

        // Cache socket ID as handshake initiator
        _cache[sid] = std::chrono::steady_clock::now() + _timeout;

        _node->enqueue_private(sid, 0, out.data(), out.size());
    }

    void enqueue_response (socket_id sid, bool behind_nat, bool accepted)
    {
        auto out = serializer_traits::make_serializer();
        handshake_packet pkt {_node->is_gateway(), behind_nat, accepted};

        pkt.id_rep = _node->id_rep();
        pkt.name = _node->name();
        pkt.serialize(out);

        _node->enqueue_private(sid, 0, out.data(), out.size());
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
                    _channels->close_channel(sid);
                    _on_expired(sid);
                } else {
                    ++pos;
                }
            }
        }

        return result;
    }

public:
    template <typename F>
    basic_handshake & on_expired (F && f)
    {
        _on_expired = std::forward<F>(f);
        return *this;
    }

    template <typename F>
    basic_handshake & on_completed (F && f)
    {
        _on_completed = std::forward<F>(f);
        return *this;
    }

    void start (socket_id sid, bool behind_nat)
    {
        enqueue_request(sid, behind_nat);
    }

    void cancel (socket_id sid)
    {
        auto erased = _cache.erase(sid) > 0;
        (void)erased;
    }

    unsigned int step ()
    {
        return check_expired();
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
