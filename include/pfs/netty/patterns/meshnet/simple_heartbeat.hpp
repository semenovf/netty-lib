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
#include <functional>
#include <set>
#include <unordered_map>
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

    // Sort by ascending order
    friend constexpr bool operator < (heartbeat_item const & lhs, heartbeat_item const & rhs)
    {
        return lhs.t < rhs.t;
    }

private:
    Node * _node {nullptr};
    std::chrono::seconds _interval {5};
    std::chrono::seconds _timeout {15};
    std::set<heartbeat_item> _q;
    std::vector<socket_id> _tmp;

    // TODO May be not optimized for large number of sockets
    std::unordered_map<socket_id, time_point_type> _limits;

    std::function<void (socket_id)> _on_expired = [] (socket_id) {};

public:
    simple_heartbeat (Node & node, std::chrono::seconds interval = std::chrono::seconds{5})
        : _node(& node)
        , _interval(interval)
    {}

private:
    void enqueue (socket_id sid)
    {
        _q.insert(heartbeat_item{std::chrono::steady_clock::now() + _interval, sid});
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

        _limits.erase(sid);
    }

    void process (socket_id sid, heartbeat_packet const & /*pkt*/)
    {
        _limits[sid] = std::chrono::steady_clock::now() + _timeout;
    }

    template <typename F>
    simple_heartbeat & on_expired (F && f)
    {
        _on_expired = std::forward<F>(f);
        return *this;
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
                _node->enqueue_private(pos->sid, 0, out.data(), out.size());
                auto sid = pos->sid;
                pos = _q.erase(pos);
                _tmp.push_back(sid);
            }

            for (auto sid: _tmp)
                enqueue(sid);

            _tmp.clear();
        }

        if (!_limits.empty()) {
            auto now = std::chrono::steady_clock::now();

            for (auto pos = _limits.begin(); pos != _limits.end();) {
                if (pos->second <= now) {
                    auto sid = pos->first;
                    pos = _limits.erase(pos);
                    remove(sid);
                    _on_expired(sid);
                } else {
                    ++pos;
                }
            }
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
