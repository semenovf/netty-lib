////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.17 Initial version (in_memory_processor.hpp).
//      2025.05.06 `im_outgoing_processor` renamed to `outgoing_controller`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../error.hpp"
#include "../../namespace.hpp"
#include "../priority_tracker.hpp"
#include "multipart_tracker.hpp"
#include <pfs/assert.hpp>
#include <chrono>
#include <cstdint>
#include <deque>
#include <queue>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

/**
 * Outgoing messages controller.
 */
template <typename MessageId
    , typename SerializerTraits
    , typename PriorityTracker = priority_tracker<single_priority_distribution>>
class outgoing_controller
{
public:
    using priority_tracker_type = PriorityTracker;

private:
    using message_id = MessageId;
    using serializer_traits = SerializerTraits;
    using clock_type = std::chrono::steady_clock;
    using time_point_type = clock_type::time_point;

    struct item
    {
        // Serial number of the last message part (enqueued)
        serial_number recent_sn {0};

        // Window/queue to track outgoing message/report parts (need access to random element)
        std::deque<multipart_tracker<message_id>> q;

        // The queue stores serial numbers for retransmission.
        std::queue<serial_number> again;
    };

private:
    // SYN packet expiration time
    time_point_type _exp_syn;

    // Serial number synchronization flag: set to true when received SYN packet response.
    bool _synchronized {false};

    std::uint32_t _part_size {0}; // Message portion size
    std::chrono::milliseconds _exp_timeout {3000}; // Expiration timeout

    priority_tracker_type _priority_tracker;
    std::array<item, PriorityTracker::SIZE> _items;

public:
    outgoing_controller (std::uint32_t part_size = 16384U // 16 Kb
        , std::chrono::milliseconds exp_timeout = std::chrono::milliseconds{3000})
        : _part_size(part_size)
        , _exp_timeout(exp_timeout)
    {}

private:
    bool syn_expired () const noexcept
    {
        return _exp_syn <= clock_type::now();
    }

    std::vector<char> acquire_syn_packet ()
    {
        _exp_syn = clock_type::now() + _exp_timeout;
        std::vector<serial_number> snumbers;
        snumbers.reserve(PriorityTracker::SIZE);

        for (auto const & x: _items) {
            serial_number syn_sn = x.recent_sn + 1;

            if (!x.q.empty())
                syn_sn = x.q.front().first_sn();

            snumbers.push_back(syn_sn);
        }

        auto out = serializer_traits::make_serializer();
        syn_packet pkt {syn_way_enum::request, std::move(snumbers)};
        pkt.serialize(out);
        return out.take();
    }

public:
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
        x.q.emplace_back(msgid, priority, force_checksum, _part_size, ++x.recent_sn, std::move(msg)
            , _exp_timeout);
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
        x.q.emplace_back(msgid, priority, force_checksum, _part_size, ++x.recent_sn, msg, length, _exp_timeout);
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
    unsigned int step (Manager * m, typename Manager::address_type receiver_addr)
    {
        unsigned int n = 0;

        // Send SYN packet to synchronize serial numbers
        if (!_synchronized) {
            if (syn_expired()) {
                int priority = 0; // High priority
                bool force_checksum = false; // No need checksum

                auto syn_packet = acquire_syn_packet();
                m->enqueue_private(receiver_addr, std::move(syn_packet), priority, force_checksum);
                n++;
            }

            return n;
        }

        if (empty())
            return n;

        // Retransmit
        for (auto & x: _items) {
            if (!x.again.empty()) {
                serial_number sn = x.again.front();
                x.again.pop();

                auto pos = multipart_tracker<message_id>::find(x.q.begin(), x.q.end(), sn);
                PFS__THROW_UNEXPECTED(pos != x.q.end(), "Fix delivery::outgoing_controller algorithm");

                auto out = serializer_traits::make_serializer();

                if (pos->acquire_part(out, sn)) {
                    m->enqueue_private(receiver_addr, out.take(), pos->priority(), pos->force_checksum());
                    n++;
                }
            }
        }

        // Candidates to retransmit were found
        if (n > 0)
            return n;

        // Check complete messages
        for (auto & x: _items) {
            auto mt = & x.q.front();

            while (mt->is_complete()) {
                m->process_message_delivered(receiver_addr, mt->msgid());

                x.q.pop_front();
                n++;

                if (x.q.empty())
                    break;

                mt = & x.q.front();
            }
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

            auto mt = & x.q.front();

            auto out = serializer_traits::make_serializer();

            if (mt->acquire_part(out)) {
                m->enqueue_private(receiver_addr, out.take(), mt->priority(), mt->force_checksum());
                n++;
                break;
            } else {
                _priority_tracker.skip();
                continue;
            }
        } while (_priority_tracker.current() != saved_priority);

        return n;
    }

    void acknowledge (int priority, serial_number sn)
    {
        auto & x = _items[priority];

        auto pos = multipart_tracker<message_id>::find(x.q.begin(), x.q.end(), sn);
        PFS__THROW_UNEXPECTED(pos != x.q.end(), "Fix delivery::outgoing_controller algorithm");
        pos->acknowledge(sn);
    }

    void again (int priority, serial_number sn)
    {
        auto & x = _items[priority];

        // Cache serial number for part retransmission in step()
        x.again.push(sn);
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
