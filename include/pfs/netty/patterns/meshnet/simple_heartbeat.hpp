////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "protocol.hpp"
#include <pfs/netty/namespace.hpp>
#include <chrono>
#include <set>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class simple_heartbeat
{
    using socket_id = typename Node::socket_id;
    using serializer_traits = typename Node::serializer_traits;
    using time_point_type = std::chrono::steady_clock::time_point;

    struct heartbeat_item
    {
        time_point_type t;
        socket_id sid;
    };

    friend constexpr bool operator < (heartbeat_item const & lhs, heartbeat_item const & rhs)
    {
        return lhs.t < rhs.t;
    }

private:
    Node & _node;
    std::chrono::seconds _timeout {5};
    std::set<heartbeat_item> _q;
    std::vector<socket_id> _tmp;

public:
    simple_heartbeat (Node & node, std::chrono::seconds timeout = std::chrono::seconds{5})
        : _node(node)
    {}

private:
    void enqueue (socket_id sid)
    {
        _q.insert(heartbeat_item{std::chrono::steady_clock::now() + _timeout, sid});
    }

public:
    void add (socket_id sid)
    {
        remove(sid);
        enqueue(sid);
    }

    void remove (socket_id sid)
    {
        for (auto pos = _q.begin(); pos != _q.end();) {
            if (pos->sid == sid)
                pos = _q.erase(pos);
            else
                ++pos;
        }
    }

    void process (socket_id sid, heartbeat_packet const & pkt)
    {
        // TODO Process Heartbeat packet
        LOGD("", "Heartbeat received: #{}: health={}", sid, pkt.health_data);
    }

    void step ()
    {
        if (!_q.empty()) {
            auto now = std::chrono::steady_clock::now();
            auto pos = _q.begin();

            auto out = serializer_traits::make_serializer();
            heartbeat_packet pkt;
            pkt.serialize(out);

            _tmp.clear();

            while (!_q.empty() && pos->t <= now) {
                _node.send(pos->sid, pkt.priority(), out.data(), out.size());
                auto sid = pos->sid;
                pos = _q.erase(pos);
                _tmp.push_back(sid);
            }

            for (auto sid: _tmp)
                enqueue(sid);

            _tmp.clear();
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
