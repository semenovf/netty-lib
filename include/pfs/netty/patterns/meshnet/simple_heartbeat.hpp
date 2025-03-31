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
#include "protocol.hpp"
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
        socket_id sid;
        time_point_type t;
    };

    // Sort by ascending order
    friend constexpr bool operator < (heartbeat_item const & lhs, heartbeat_item const & rhs)
    {
        return lhs.t < rhs.t;
    }

private:
    Node * _node {nullptr};

    // Expiration timeout
    std::chrono::seconds _exp_timeout {15};

    std::chrono::seconds _interval {5};

    std::set<heartbeat_item> _q;
    std::vector<socket_id> _tmp;

    // TODO May be not optimized for large number of sockets
    std::unordered_map<socket_id, time_point_type> _limits;

    std::function<void (socket_id)> _on_expired = [] (socket_id) {};

public:
    simple_heartbeat (Node * node
        , std::chrono::seconds exp_timeout = std::chrono::seconds{15}
        , std::chrono::seconds interval = std::chrono::seconds{5})
        : _node(node)
        , _exp_timeout(exp_timeout)
        , _interval(interval)
    {}

private:
    void insert (socket_id sid)
    {
        _q.insert(heartbeat_item{sid, std::chrono::steady_clock::now() + _interval});
    }

public:
    void update (socket_id sid)
    {
        remove(sid);
        insert(sid);
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
        _limits[sid] = std::chrono::steady_clock::now() + _exp_timeout;
    }

    template <typename F>
    simple_heartbeat & on_expired (F && f)
    {
        _on_expired = std::forward<F>(f);
        return *this;
    }

    unsigned int step ()
    {
        unsigned int result = 0;

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

            for (auto sid: _tmp) {
                insert(sid);
                result++;
            }

            _tmp.clear();
        }

        if (!_limits.empty()) {
            auto now = std::chrono::steady_clock::now();

            for (auto pos = _limits.begin(); pos != _limits.end();) {
                if (pos->second <= now) {
                    result++;
                    auto sid = pos->first;
                    pos = _limits.erase(pos);
                    remove(sid);
                    _on_expired(sid);
                } else {
                    ++pos;
                }
            }
        }

        return result;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
