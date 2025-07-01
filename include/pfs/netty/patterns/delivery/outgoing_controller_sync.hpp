////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.07.70 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../error.hpp"
#include "../../namespace.hpp"
#include "../../tag.hpp"
#include "../../trace.hpp"
#include "../priority_tracker.hpp"
#include "multipart_tracker.hpp"
#include <pfs/assert.hpp>
#include <pfs/optional.hpp>
#include <chrono>
#include <cstdint>
#include <queue>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

/**
 * Outgoing messages controller.
 */
template <typename Address
    , typename MessageId
    , typename SerializerTraits
    , typename PriorityTracker = priority_tracker<single_priority_distribution>>
class outgoing_controller_sync
{
public:
    using priority_tracker_type = PriorityTracker;

private:
    using address_type = Address;
    using message_id = MessageId;
    using serializer_traits = SerializerTraits;
    using clock_type = std::chrono::steady_clock;
    using time_point_type = clock_type::time_point;

    struct item
    {
        // Serial number of the last message part of the last message in the queue
        serial_number recent_sn {0};

        // Serial number of the current enqueued part
        serial_number current_sn {0};

        // Serial number of the last acknowledged part
        // acked_sn <= current_sn
        serial_number acked_sn {0};

        // Queue to track of the outgoing messages
        std::queue<multipart_tracker<message_id>> q;
    };

private:
    address_type _receiver_addr;

    // SYN packet expiration time
    time_point_type _exp_syn;

    // Serial number synchronization flag: set to true when received SYN packet response.
    bool _synchronized {false};

    std::uint32_t _part_size {0}; // Message portion size
    std::chrono::milliseconds _exp_timeout {3000}; // Expiration timeout

    priority_tracker_type _priority_tracker;
    std::array<item, PriorityTracker::SIZE> _items;

    bool _paused {false};

public:
    outgoing_controller_sync (address_type receiver_addr, std::uint32_t part_size = 16384U // 16 Kb
        , std::chrono::milliseconds exp_timeout = std::chrono::milliseconds{3000})
        : _receiver_addr(receiver_addr)
        , _part_size(part_size)
        , _exp_timeout(exp_timeout)
    {}

private:
    bool syn_expired () const noexcept
    {
        return _exp_syn <= clock_type::now();
    }

    std::vector<char> acquire_syn_packet ()
    {
        std::vector<serial_number> snumbers;
        snumbers.reserve(PriorityTracker::SIZE);

        for (auto const & x: _items)
            snumbers.push_back(x.acked_sn + 1);

        auto out = serializer_traits::make_serializer();
        syn_packet pkt {syn_way_enum::request, std::move(snumbers)};
        pkt.serialize(out);

        _exp_syn = clock_type::now() + _exp_timeout;

        return out.take();
    }

public:
    bool paused () const noexcept
    {
        return _paused;
    }

    void pause () noexcept
    {
        _paused = true;
        NETTY__TRACE(TAG, "Message sending has been paused to: {}", to_string(_receiver_addr));
    }

    void resume () noexcept
    {
        _synchronized = false;
        _paused = false;
        NETTY__TRACE(TAG, "Message sending has been resumed to: {}", to_string(_receiver_addr));
    }

    void set_synchronized (bool value) noexcept
    {
        _synchronized = value;
    }

    /**
     * Enqueues regular message.
     */
    bool enqueue_message (message_id msgid, int priority, bool force_checksum, std::vector<char> msg)
    {
        auto & x = _items[priority];
        x.q.emplace(msgid, priority, force_checksum, _part_size, ++x.recent_sn, std::move(msg), _exp_timeout);
        auto & mt = x.q.back();

        x.recent_sn = mt.last_sn();

        return true;
    }

    /**
     * Enqueues regular message.
     */
    bool enqueue_static_message (message_id msgid, int priority, bool force_checksum
        , char const * msg, std::size_t length)
    {
        auto & x = _items[priority];
        x.q.emplace(msgid, priority, force_checksum, _part_size, ++x.recent_sn, msg, length, _exp_timeout);
        auto & mt = x.q.back();
        x.recent_sn = mt.last_sn();
        return true;
    }

    /**
     * Checks whether no messages to transmit.
     */
    bool empty () const
    {
        for (auto const & x: _items) {
            if (!x.q.empty())
                return false;
        }

        return true;
    }

    template <typename Manager>
    unsigned int step (Manager * m)
    {
        unsigned int n = 0;

        // Send SYN packet to synchronize serial numbers
        if (!_synchronized) {
            if (syn_expired()) {
                int priority = 0; // High priority
                bool force_checksum = false; // No need checksum

                auto serialized_syn_packet = acquire_syn_packet();
                auto success = m->enqueue_private(_receiver_addr, std::move(serialized_syn_packet)
                    , priority, force_checksum);

                if (success) {
                    n++;
                } else {
                    // There is a problem in communication with the receiver
                    _paused = true;
                }
            }

            return n;
        }

        if (empty())
            return n;

        auto saved_priority = _priority_tracker.current();

        // Try to acquire next part of the current sending message according to priority
        do {
            auto priority = _priority_tracker.next();

            auto & x = _items[priority];

            if (x.q.empty()) {
                _priority_tracker.skip();
                continue;
            }

            if (x.acked_sn < x.current_sn) {
                _priority_tracker.skip();
                continue;
            }

            auto mt = & x.q.front();

            auto out = serializer_traits::make_serializer();
            auto res_sn = mt->acquire_next_part(out);

            if (res_sn > 0) {
                x.current_sn = res_sn;

                auto success = m->enqueue_private(_receiver_addr, out.take(), mt->priority()
                    , mt->force_checksum());

                if (success) {
                    n++;
                } else {
                    // There is a problem in communication with the receiver
                    _paused = true;
                }

                break;
            } else {
                _priority_tracker.skip();
                continue;
            }
        } while (_priority_tracker.current() != saved_priority);

        return n;
    }

    pfs::optional<message_id> acknowledge (int priority, serial_number sn)
    {
        auto & x = _items[priority];
        x.acked_sn = sn;
        auto mt = & x.q.front();

        PFS__THROW_UNEXPECTED(mt->check_range(sn)
            , "Fix delivery::outgoing_controller_sync algorithm: serial number is out of bounds");

        if (!mt->acknowledge(sn))
            return pfs::nullopt;

        // The message has been delivered completely
        auto msgid = mt->msgid();
        x.q.pop();

        return msgid;
    }

    void again (int priority, serial_number first_sn, serial_number last_sn)
    {
        PFS__THROW_UNEXPECTED(false
            , "Fix delivery::outgoing_controller_sync algorithm: unexpected call `again()`");
    }

public: // static
    static std::vector<char> serialize_report (char const * data, std::size_t length)
    {
        auto out = serializer_traits::make_serializer();
        report_packet pkt;
        pkt.serialize(out, data, length);
        return out.take();
    }

    static std::vector<char> serialize_report (std::vector<char> && data)
    {
        return serialize_report(data.data(), data.size());
    }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
