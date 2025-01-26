////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.25 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "handshake_result.hpp"
#include "protocol.hpp"
#include <pfs/netty/namespace.hpp>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <template <typename> class Derived, typename Node>
class basic_handshake
{
protected:
    using node_id = typename Node::node_id;
    using socket_id = typename Node::socket_id;
    using node_idintifier_traits = typename Node::node_idintifier_traits;
    using serializer_traits = typename Node::serializer_traits;
    using time_point_type = std::chrono::steady_clock::time_point;

protected:
    Node & _node;
    std::map<socket_id, time_point_type> _cache;
    std::chrono::seconds _timeout {10};

    mutable std::function<void(socket_id)> _on_expired;
    mutable std::function<void(socket_id, std::string const &)> _on_failure;
    mutable std::function<void(node_id, socket_id, handshake_result_enum)> _on_completed;

public:
    basic_handshake (Node & node)
        : _node(node)
    {}

protected:
    void send (socket_id sid, packet_way_enum way)
    {
        auto out = serializer_traits::make_serializer();
        handshake_packet pkt {way};
        pkt.id = to_string(_node.id());
        pkt.serialize(out);

        // Cache socket ID as handshake initiator
        if (way == packet_way_enum::request)
            _cache[sid] = std::chrono::steady_clock::now() + _timeout;

        _node.send(sid, pkt.priority(), out.data(), out.size());
    }

    void check_expired ()
    {
        if (!_cache.empty()) {
            auto now = std::chrono::steady_clock::now();

            for (auto pos = _cache.begin(); pos != _cache.end();) {
                if (pos->second <= now) { // Time limit exceeded
                    auto sid = pos->first;
                    pos = _cache.erase(pos);
                    _on_expired(sid);
                } else {
                    ++pos;
                }
            }
        }
    }

public:
    template <typename F>
    Derived<Node> & on_expired (F && f)
    {
        _on_expired = std::forward<F>(f);
        return *static_cast<Derived<Node> *>(this);
    }

    template <typename F>
    Derived<Node> & on_failure (F && f)
    {
        _on_failure = std::forward<F>(f);
        return *static_cast<Derived<Node> *>(this);
    }

    template <typename F>
    Derived<Node> & on_completed (F && f)
    {
        _on_completed = std::forward<F>(f);
        return *static_cast<Derived<Node> *>(this);
    }

    void start (socket_id sid)
    {
        send(sid, packet_way_enum::request);
    }

    void cancel (socket_id sid)
    {
        auto erased = _cache.erase(sid) > 0;
        (void)erased;
    }

    void process (socket_id sid, handshake_packet const & pkt)
    {
        auto optid = node_idintifier_traits::parse(pkt.id.data(), pkt.id.size());

        if (optid) { // Valid node ID received
            // If item not found, it means it is already expired
            auto pos = _cache.find(sid);

            if (pos != _cache.end()) { // Received response
                cancel(sid);
                static_cast<Derived<Node> *>(this)->nodeid_ready(sid, *optid, packet_way_enum::response);
            } else {
                // Received from handshake initiator
                if (!pkt.is_response()) {
                    send(sid, packet_way_enum::response);
                    static_cast<Derived<Node> *>(this)->nodeid_ready(sid, *optid, packet_way_enum::request);
                }
                // else the socket is already expired
            }
        } else { // Invalid node ID received
            cancel(sid);
            _on_failure(sid, tr::f_("bad node identifier received from: #{}", sid));
        }
    }

    void step ()
    {
        check_expired();
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
