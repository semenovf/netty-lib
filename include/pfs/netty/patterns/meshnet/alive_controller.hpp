////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.13 Initial version.
//      2025.05.07 Renamed from alive_processor.hpp to alive_controller.hpp.
//                 Class `alive_processor` renamed to `alive_controller`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../callback.hpp"
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
class alive_controller
{
    using node_id_traits = NodeIdTraits;
    using serializer_traits = SerializerTraits;
    using node_id = typename node_id_traits::type;
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
    node_id _id;

    // Expiration timeout
    std::chrono::seconds _exp_timeout {15}; // default is _interval * 3

    // Interval between notifications (sending alive packets)
    std::chrono::seconds _interval {5};

    std::chrono::milliseconds _looping_interval {2500}; // default is _interval/2

    time_point_type _next_notification_time;

    // Direct access nodes. No need to control alive by timeout expiration
    std::unordered_set<node_id> _sibling_nodes;

    // Non-direct access nodes.
    std::unordered_set<node_id> _alive_nodes;

    std::set<alive_item> _alive_items;

public:
    callback_t<void (node_id)> on_alive = [] (node_id) {};
    callback_t<void (node_id)> on_expired = [] (node_id) {};

public:
    alive_controller (node_id id, std::chrono::seconds exp_timeout = std::chrono::seconds{15}
        , std::chrono::seconds interval = std::chrono::seconds{5}
        , std::chrono::milliseconds looping_interval = std::chrono::milliseconds{2500})
        : _id(id)
        , _exp_timeout(exp_timeout)
        , _interval(interval)
        , _looping_interval(looping_interval)
        , _next_notification_time(std::chrono::steady_clock::now())
    {}

    alive_controller (alive_controller const &) = delete;
    alive_controller (alive_controller &&) = delete;
    alive_controller & operator = (alive_controller const &) = delete;
    alive_controller & operator = (alive_controller &&) = delete;

    ~alive_controller () = default;

private:
    void insert (node_id id)
    {
        auto now = std::chrono::steady_clock::now();
        _alive_items.insert(alive_item{id, now + _exp_timeout, now + _looping_interval});
        _alive_nodes.insert(id);
    }

public:
    void add_sibling (node_id id)
    {
        _sibling_nodes.insert(id);
        this->on_alive(id);
    }

    /**
     * Expires the node @a id.
     *
     * @param id Expired node identifier.
     *
     * @details Call this method when need to force node expiration, e.g. when node unreachable
     *          notification received .
     */
    void expire (node_id id)
    {
        auto count = _sibling_nodes.erase(id);

        if (count == 0) {
            count = _alive_nodes.erase(id);

            if (count > 0) {
                for (auto pos = _alive_items.begin(); pos != _alive_items.end(); ++pos) {
                    if (pos->id == id) {
                        _alive_items.erase(pos);
                        break;
                    }
                }
            }
        }

        if (count > 0)
            this->on_expired(id);
    }

    /**
     * Updates node's alive info if now time point is less than looping_threshold
     */
    bool update_if (node_id id)
    {
        auto pos = _sibling_nodes.find(id);

        // Sibling node, no need to update
        if (pos != _sibling_nodes.end())
            return true;

        auto now = std::chrono::steady_clock::now();

        for (auto pos = _alive_items.begin(); pos != _alive_items.end(); ++pos) {
            if (pos->id == id) {
                // Looping or duplication detected
                if (now < pos->looping_threshold)
                    return false;

                // Erase only from _alive_items to insert new value in insert() method below.
                // No need to erase from _alive_nodes here.
                _alive_items.erase(pos);

                insert(id);
                return true;
            }
        }

        // New alive node detected
        insert(id);

        this->on_alive(id);

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

        if (_alive_nodes.find(id) != _alive_nodes.end())
            return true;

        return false;
    }

    std::vector<char> serialize_alive ()
    {
        auto out = serializer_traits::make_serializer();
        alive_packet<node_id> pkt;
        pkt.ainfo.id = _id;
        pkt.serialize(out);
        return out.take();
    }

    std::vector<char> serialize_alive (alive_info<node_id> const & ainfo)
    {
        auto out = serializer_traits::make_serializer();
        alive_packet<node_id> pkt;
        pkt.ainfo = ainfo;
        pkt.serialize(out);
        return out.take();
    }

    /**
     * Serializes initial custom message.
     */
    std::vector<char> serialize_unreachable (node_id gw_id, node_id sender_id, node_id receiver_id)
    {
        auto out = serializer_traits::make_serializer();
        unreachable_packet<node_id> pkt;
        pkt.uinfo.gw_id = gw_id;
        pkt.uinfo.sender_id = sender_id;
        pkt.uinfo.receiver_id = receiver_id;
        pkt.serialize(out);
        return out.take();
    }

    std::vector<char> serialize_unreachable (unreachable_info<node_id> const & uinfo)
    {
        auto out = serializer_traits::make_serializer();
        unreachable_packet<node_id> pkt;
        pkt.uinfo = uinfo;
        pkt.serialize(out);
        return out.take();
    }

    void check_expiration ()
    {
        if (!_alive_items.empty()) {
            auto now = std::chrono::steady_clock::now();
            auto pos = _alive_items.begin();

            while (!_alive_items.empty() && pos->exp_time <= now) {
                _alive_nodes.erase(pos->id);
                this->on_expired(pos->id);
                pos = _alive_items.erase(pos);
            }
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
