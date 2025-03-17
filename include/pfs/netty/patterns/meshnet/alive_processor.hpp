////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "alive_info.hpp"
#include "protocol.hpp"
#include <pfs/assert.hpp>
#include <set>
#include <chrono>
#include <cstdint>
#include <unordered_set>
#include <utility>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

//
// The algorithm is similar to heartbeat algorithm
//
template <typename NodeIdTraits, typename SerializerTraits>
class alive_processor
{
    using node_id = typename NodeIdTraits::node_id;
    using serializer_traits = SerializerTraits;
    using time_point_type = std::chrono::steady_clock::time_point;

    struct alive_item
    {
        node_id id;
        time_point_type exp_time;  // expiration timepoint
        time_point_type looping_threshold; // a value within this variable indicates duplication or looping
    };

    // Sort by ascending order
    friend constexpr bool operator < (alive_item const & lhs, alive_item const & rhs)
    {
        return lhs.exp_time < rhs.exp_time;
    }

private:
    // Node ID representation
    std::pair<std::uint64_t, std::uint64_t> _id_pair;

    // Expiration timeout
    std::chrono::seconds _exp_timeout {15}; // default is _interval * 3

    // Interval between notifications (sending alive packets)
    std::chrono::seconds _interval {5};

    std::chrono::milliseconds _looping_interval {2500}; // default is _interval/2

    time_point_type _next_notification_time;

    // Direct access nodes. No need to control alive by timeout expiration
    std::unordered_set<node_id> _sibling_nodes;

    std::set<alive_item> _q;

    std::function<void (node_id)> _on_alive = [] (node_id) {};
    std::function<void (node_id)> _on_expired = [] (node_id) {};

public:
    alive_processor (node_id id, std::chrono::seconds exp_timeout = std::chrono::seconds{15}
        , std::chrono::seconds interval = std::chrono::seconds{5}
        , std::chrono::milliseconds looping_interval = std::chrono::milliseconds{2500})
        : _exp_timeout(exp_timeout)
        , _interval(interval)
        , _looping_interval(looping_interval)
    {
        _id_pair.first = NodeIdTraits::high(id);
        _id_pair.second = NodeIdTraits::low(id);
        _next_notification_time = std::chrono::steady_clock::now();
    }

    alive_processor (alive_processor const &) = delete;
    alive_processor (alive_processor &&) = delete;
    alive_processor & operator = (alive_processor const &) = delete;
    alive_processor & operator = (alive_processor &&) = delete;

    ~alive_processor () = default;

private:
    void insert (node_id id)
    {
        auto now = std::chrono::steady_clock::now();
        _q.insert(alive_item{id, now + _exp_timeout, now + _looping_interval});
    }

public:
    template <typename F>
    alive_processor & on_alive (F && f)
    {
        _on_alive = std::forward<F>(f);
        return *this;
    }

    template <typename F>
    alive_processor & on_expired (F && f)
    {
        _on_expired = std::forward<F>(f);
        return *this;
    }

    void add_sibling (node_id id)
    {
        _sibling_nodes.insert(id);
        _on_alive(id);
    }

    void remove_sibling (node_id id)
    {
        auto erased_count = _sibling_nodes.erase(id);
        PFS__TERMINATE(erased_count == 1, "Fix meshnet::alive_processor algorithm");
        _on_expired(id);
    }

    /**
     * Updates node's alive info if now time point is less than looping_threshold
     */
    bool update_if (node_id id)
    {
        auto pos = _sibling_nodes.find(id);

        // Sibling node, no need to update
        if (pos != _sibling_nodes.end())
            return false;

        auto now = std::chrono::steady_clock::now();

        for (auto pos = _q.begin(); pos != _q.end(); ++pos) {
            if (pos->id == id) {
                // Looping or duplication detected
                if (now < pos->looping_threshold)
                    return false;

                _q.erase(pos);
                insert(id);
                return true;
            }
        }

        // New alive node detected
        insert(id);

        _on_alive(id);

        return true;
    }

    bool interval_exceeded () const noexcept
    {
        return std::chrono::steady_clock::now() >= _next_notification_time;
    }

    void update_notification_time () noexcept
    {
        _next_notification_time = std::chrono::steady_clock::now() + _interval;
    }

    bool is_alive (node_id id) const
    {
        if (_sibling_nodes.find(id) != _sibling_nodes.end())
            return true;

        auto pos = _q.find(id);

        if (pos == _q.end())
            return false;

        auto now = std::chrono::steady_clock::now();
        return pos->t > now;
    }

    std::vector<char> serialize ()
    {
        auto out = serializer_traits::make_serializer();
        alive_packet pkt;
        pkt.ainfo.id = _id_pair;
        pkt.serialize(out);
        return out.take();
    }

    std::vector<char> serialize (alive_info const & ainfo)
    {
        auto out = serializer_traits::make_serializer();
        alive_packet pkt;
        pkt.ainfo = ainfo;
        pkt.serialize(out);
        return out.take();
    }

    void check_expiration ()
    {
        if (!_q.empty()) {
            auto now = std::chrono::steady_clock::now();
            auto pos = _q.begin();

            while (!_q.empty() && pos->exp_time <= now) {
                _on_expired(pos->id);
                pos = _q.erase(pos);
            }
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
