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
    , typename SerializerTraits>
class outgoing_controller
{
    using message_id = MessageId;
    using serializer_traits = SerializerTraits;
    using clock_type = std::chrono::steady_clock;
    using time_point_type = clock_type::time_point;

private:
    // SYN packet expiration time
    time_point_type _exp_syn;

    // Serial number synchronization flag: set to true when received SYN packet response.
    bool _synchronized {false};

    serial_number _recent_sn {0}; // Serial number of the last message part (enqueued)
    std::uint32_t _part_size {0}; // Message portion size
    std::chrono::milliseconds _exp_timeout {3000}; // Expiration timeout

    // Window/queue to track outgoing message/report parts (need access to random element)
    std::deque<multipart_tracker<message_id>> _q;

    // The queue stores serial numbers for retransmission.
    std::queue<serial_number> _again;

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
        serial_number syn_sn = _recent_sn + 1;

        if (!_q.empty())
            syn_sn = _q.front().first_sn();

        auto out = serializer_traits::make_serializer();
        syn_packet pkt {syn_way_enum::request, syn_sn};
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
    bool enqueue_message (message_id msgid, int priority, bool force_checksum, std::vector<char> && msg)
    {
        _q.emplace_back(msgid, priority, force_checksum, _part_size, ++_recent_sn, std::move(msg), _exp_timeout);
        auto & mt = _q.back();
        _recent_sn = mt.last_sn();

        return true;
    }

    /**
     * Enqueues regular message.
     */
    bool enqueue_static_message (message_id msgid, int priority, bool force_checksum
        , char const * msg, std::size_t length)
    {
        _q.emplace_back(msgid, priority, force_checksum, _part_size, ++_recent_sn, msg, length, _exp_timeout);
        auto & mt = _q.back();
        _recent_sn = mt.last_sn();
        return true;
    }

    /**
     * Checks whether no messages to transmit.
     */
    bool empty () const
    {
        return _q.empty();
    }

    /**
     */
    template <typename OnSend, typename OnDispatched>
    unsigned int step (OnSend && on_send, OnDispatched && on_delivered)
    {
        unsigned int n = 0;

        // Send SYN packet to synchronize serial numbers
        if (!_synchronized) {
            if (syn_expired()) {
                int priority = 0; // High priority
                bool force_checksum = false; // No need checksum

                auto syn_packet = acquire_syn_packet();
                on_send(priority, force_checksum, std::move(syn_packet));
                n++;
            }

            return n;
        }

        if (_q.empty())
            return n;

        // Retransmit
        if (!_again.empty()) {
            while (!_again.empty()) {
                serial_number sn = _again.front();
                _again.pop();

                auto pos = multipart_tracker<message_id>::find(_q.begin(), _q.end(), sn);
                PFS__THROW_UNEXPECTED(pos != _q.end(), "Fix delivery::outgoing_controller algorithm");

                auto out = serializer_traits::make_serializer();

                if (pos->acquire_part(out)) {
                    on_send(pos->priority(), pos->force_checksum(), out.take());
                    n++;
                }
            }

            return n;
        }

        // Check complete messages
        auto mt = & _q.front();

        while (mt->is_complete()) {
            on_delivered(mt->msgid());

            _q.pop_front();
            n++;

            if (_q.empty())
                break;

            mt = & _q.front();
        }

        if (_q.empty())
            return n;

        // Try to acquire next part of the current sending message
        mt = & _q.front();

        auto out = serializer_traits::make_serializer();

        if (mt->acquire_part(out)) {
            on_send(mt->priority(), mt->force_checksum(), out.take());
            n++;
        }

        return n;
    }

    void acknowledge (serial_number sn)
    {
        auto pos = multipart_tracker<message_id>::find(_q.begin(), _q.end(), sn);
        PFS__THROW_UNEXPECTED(pos != _q.end(), "Fix delivery::outgoing_controller algorithm");
        pos->acknowledge(sn);
    }

    void again (serial_number sn)
    {
        // Cache serial number for part retransmission in step()
        _again.push(sn);
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
