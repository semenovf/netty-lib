////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "multipart_assembler.hpp"
#include "multipart_tracker.hpp"
#include <pfs/assert.hpp>
#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <queue>
#include <utility>
#include <vector>

// FIXME REMOVE
#include <pfs/log.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

/**
 * In-memory income messages processor
 */
template <typename MessageIdTraits
    , typename SerializerTraits>
class im_incoming_processor
{
    using message_id_traits = MessageIdTraits;
    using message_id = typename message_id_traits::type;
    using serializer_traits = SerializerTraits;

    struct account
    {
        pfs::optional<message_id> msgid_opt; // Valid for regular message
        serial_number first_sn {0};
        serial_number last_sn {0};
    };

private:
    // Bounds for sliding window
    //
    // last committed SN
    //           |
    //           |     window
    //           | |<--------->|
    //           v |           |
    // +--+--+--+--+--+--+--+--+--+--+--+--+--+
    // |CC|CC|CC|CC|pp|  |pp|pp|  |  |  |  |  |
    // +--+--+--+--+--+--+--+--+--+--+--+--+--+
    //                       ^  ^
    //                       |  |
    //                       |  expected SN
    //    last income message SN (recent_sn)
    //

    serial_number _committed_sn {0}; // Last serial number of the last committed income message
    serial_number _expected_sn {0};  // Expected income message part serial number

    std::deque<account> _window;

    std::map<message_id, multipart_assembler> _assemblers;

public:
    im_incoming_processor ()
    {}

public:
    template <typename OnSend, typename OnReady, typename OnMessageReceived
        , typename OnAcknowledged>
    void process_packet (std::vector<char> && data, OnSend && on_send, OnReady && on_ready
        , OnMessageReceived && on_message_received, OnAcknowledged && on_acknowledged)
    {
        auto in = serializer_traits::make_deserializer(data.data(), data.size());
        in.start_transaction();

        // Data can contains more than one packet (see again() method for group of packets)
        do {
            header h {in};

            if (in.is_good()) {
                switch (h.type()) {
                    case packet_enum::syn: {
                        syn_packet pkt {h, in};

                        if (pkt.is_request()) {
                            _expected_sn = pkt.sn();
                            auto out = serializer_traits::make_serializer();
                            syn_packet response_pkt {syn_way_enum::response, pkt.sn()};
                            response_pkt.serialize(out);
                            on_send(out.take());
                        } else {
                            on_ready();
                        }

                        break;
                    }

                    case packet_enum::message:
                    case packet_enum::report: {
                        std::vector<char> part;
                        message_packet pkt {h, in, part};

                        if (!in.commit_transaction())
                            break;

                        auto msgid_opt = message_id_traits::parse(pkt.msgid);

                        // Bad message ID received
                        if (!msgid_opt)
                            break;

                        multipart_assembler * assembler = nullptr;
                        auto pos = _assemblers.find(*msgid_opt);

                        // Already exists, check credentials equality.
                        if (pos != _assemblers.end()) {
                            assembler = & pos->second;

                            auto success = assembler->is_equal_credentials(pkt.total_size
                                , pkt.part_size, pkt.sn(), pkt.last_sn);

                            // What needs to be done? Erase old data and start again? In such case
                            // the sender must be notified to start transmitting the message again.
                            // Do we need a new packet type or should we consider this an unexpected
                            // situation? Let's consider this an unexpected situation for now.
                            PFS__TERMINATE(success, "Fix meshnet::im_incoming_processor algorithm");
                        }

                        if (assembler == nullptr) {
                            auto res = _assemblers.emplace(*msgid_opt, multipart_assembler{
                                pkt.total_size, pkt.part_size, pkt.sn(), pkt.last_sn});

                            PFS__ASSERT(res.second, "");

                            assembler = & res.first->second;
                        }

                        assembler->emplace_part(pkt.sn(), std::move(part));

                        // ACK for regular message only
                        if (h.type() == packet_enum::message) {
                            // Send ACK packet
                            auto out = serializer_traits::make_serializer();
                            ack_packet ack_pkt {pkt.sn()};
                            ack_pkt.serialize(out);
                            on_send(out.take());
                        }

                        // Single part message/report
                        if (assembler->is_complete()) {
                            on_message_received(assembler->payload());
                            _committed_sn = assembler->last_sn();
                            _assemblers.erase(*msgid_opt);
                        } else {
                            _window.push_back(account{msgid_opt, pkt.sn(), pkt.last_sn});
                        }

                        break;
                    }

                    case packet_enum::ack: {
                        ack_packet pkt {h, in};

                        if (!in.commit_transaction())
                            break;

                        on_acknowledged(pkt.sn());

                        break;
                    }

    //                 case packet_enum::nack: {
    //                     LOGD(_name.c_str(), "RCV: NACK: sid={}", h.id());
    //                     nack_packet pkt {h, in};
    //
    //                     // The processing is the same as for `ack`
    //                     if (in.commit_transaction())
    //                         _outproc->ack(h.id());
    //
    //                     break;
    //                 }
    //
    //                 case packet_enum::again: {
    //                     LOGD(_name.c_str(), "RCV: AGAIN: sid={}", h.id());
    //
    //                     ack_packet pkt {h, in};
    //
    //                     if (in.commit_transaction()) {
    //                         auto msg = _outproc->payload(h.id());
    //                         _callbacks.dispatch(std::move(msg));
    //                     }
    //                     break;
    //                 }
    //
    //                 default:
    //                     throw error {tr::f_("unexpected packet type: {}", pfs::to_underlying(h.type()))};
    //
    //                     break;
                }
            }

            if (!in.is_good()) {
                throw error {tr::f_("bad or corrupted header for reliable delivery packet")};
            }
        } while (in.available() > 0);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// im_outgoing_processor
////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * In-memory outgoing messages processor
 */
template <typename MessageIdTraits
    , typename SerializerTraits>
class im_outgoing_processor
{
    using message_id_traits = MessageIdTraits;
    using message_id = typename message_id_traits::type;
    using serializer_traits = SerializerTraits;
    using clock_type = std::chrono::steady_clock;
    using time_point_type = clock_type::time_point;

private:
    // Bounds for sliding window
    //
    // last acknowledged serial number (ack_sn)
    //           |
    //           |   Window
    //           | |<------>|
    //           v |        |
    // +--+--+--+--+--+--+--+--+--+--+--+--+--+
    // |AA|AA|AA|AA|pp|pp|pp|  |  |  |  |  |  |
    // +--+--+--+--+--+--+--+--+--+--+--+--+--+
    //                    ^
    //                    |
    //    last outcome message serial ID (recent_sn)
    //
    // ack_sn = recent_sn - window_size
    //

    // SYN packet expiration time
    time_point_type _exp_syn;

    // Serial number synchronization flag: set to true when received SYN packet response.
    bool _synchronized {false};

    serial_number _recent_sn {0}; // Serial number of the last message part
    std::uint32_t _part_size {0};   // Message portion size
    std::chrono::milliseconds _exp_timeout {3000}; // Expiration timeout

    // Window/queue to track outgoing message/report parts (need access to random element)
    std::deque<multipart_tracker> _q;

public:
    im_outgoing_processor (std::uint32_t part_size = 2048U // 32767U // FIXME Small value for test purposes
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

        auto out = serializer_traits::make_serializer();
        syn_packet pkt {syn_way_enum::request, _recent_sn + 1};
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
        _q.emplace_back(message_id_traits::to_string(msgid), priority, force_checksum
            , _part_size, ++_recent_sn, std::move(msg), _exp_timeout);
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
        _q.emplace_back(message_id_traits::to_string(msgid), priority, force_checksum
            , _part_size, ++_recent_sn, msg, length, _exp_timeout);
        auto & mt = _q.back();
        _recent_sn = mt.last_sn();
        return true;
    }

    /**
     * Enqueues the report.
     */
    bool enqueue_report (int priority, bool force_checksum, std::vector<char> && msg)
    {
        _q.emplace_back(priority, force_checksum, _part_size, ++_recent_sn, std::move(msg));
        auto & mt = _q.back();
        _recent_sn = mt.last_sn();
        return true;
    }

    /**
     * Enqueues the report.
     */
    bool enqueue_static_report (int priority, bool force_checksum, char const * msg, std::size_t length)
    {
        _q.emplace_back(priority, force_checksum, _part_size, ++_recent_sn, msg, length);
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
    unsigned int step (OnSend && on_send, OnDispatched && on_dispatched)
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

        // Check complete messages
        auto & mt = _q.front();

        while (mt.is_complete()) {
            if (!mt.is_report()) {
                auto msgid_opt = message_id_traits::parse(mt.msgid());

                if (msgid_opt)
                    on_dispatched(*msgid_opt);
            }

            _q.pop_front();
            n++;

            if (_q.empty())
                break;

            mt = _q.front();
        }

        if (_q.empty())
            return n;

        // Try to acquire next part of the current sending message
        mt = _q.front();

        auto out = serializer_traits::make_serializer();

        if (mt.acquire_part(out)) {
            on_send(mt.priority(), mt.force_checksum(), out.take());
            n++;
        }

        return n;
    }

    // std::vector<char> acquire_part (int * priority, bool * force_checksum)
    // {
    //
    //     // First search expired part.
    //     for (auto & mt: _q) {
    //         // Found
    //         if (!acc.acked && acc.exp_time <= clock_type::now()) {
    //             acc.exp_time = clock_type::now() + _exp_timeout;
    //             auto pos = multipart_tracker::find(acc.sn, _q.begin(), _q.end());
    //
    //             PFS__TERMINATE(pos != _q.end(), "Fix meshnet::im_outgoing_processor algorithm");
    //
    //             auto out = serializer_traits::make_serializer();
    //             pos->serialize(out, acc.sn);
    //             return out.take();
    //         }
    //     }
    //
    //     // No data
    //     if (_q.empty())
    //         return std::vector<char>{};
    //
    //     // Window is full
    //     if (_window.size() >= _window_size)
    //         return std::vector<char>{};
    //
    //     // Acquire new part
    //     //
    //     // Search message that contains serial number of the last part from window incrementing
    //     // by one - next part to transmit.
    //     serial_number sn = 0;
    //
    //     if (!_window.empty()) {
    //         auto const & acc = _window.back();
    //         sn = acc.sn + 1;
    //     } else {
    //         sn = _q.front().first_sn();
    //     }
    //
    //     auto pos = multipart_tracker::find(sn, _q.begin(), _q.end());
    //
    //     PFS__TERMINATE(pos != _q.end(), "Fix meshnet::im_outgoing_processor algorithm");
    //
    //     _window.emplace_back(sn, clock_type::now() + _exp_timeout);
    //
    //     auto out = serializer_traits::make_serializer();
    //     pos->serialize(out, sn);
    //     return out.take();
    // }

    void acknowledge (serial_number sn)
    {
        auto pos = multipart_tracker::find(_q.begin(), _q.end(), sn);
        PFS__TERMINATE(pos != _q.end(), "Fix meshnet::im_outgoing_processor algorithm");
        pos->acknowledge(sn);
    }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
