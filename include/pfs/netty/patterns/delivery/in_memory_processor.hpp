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
        message_id msgid;
        serial_number initial_sn {0};
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

    // serial_number _committed_sn {0}; // Last serial number of the last committed income message
    serial_number _expected_sn {0};    // Last income message part serial number

    std::deque<account> _window;

    std::map<message_id, multipart_assembler> _assemblers;

public:
    im_incoming_processor ()
    {}

private:
    // /**
    //  * New message part received.
    //  */
    // bool is_part (serial_number sn) const noexcept
    // {
    //     return sn > _recent_sn;
    // }
    //
    // /**
    //  * Message has already been received completely or specified by @a sn part only.
    //  */
    // bool is_acknowledged (serial_number sn) const noexcept
    // {
    //     if (sn <= _committed_sn)
    //         return true;
    //
    //     for (auto const & acc: _window) {
    //         if (acc.sn == sn) {
    //             if (acc.acked)
    //                 return true;
    //             break;
    //         }
    //     }
    //
    //     return false;
    // }

public:
    template <typename OnSend, typename OnReady>
    void process_packet (std::vector<char> && data, OnSend && on_send, OnReady && on_ready)
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




                        // // New message part received (first part)
                        // if (is_part(h.sn())) {
                        //     auto pos = _assemblers.find(*msgid_opt);
                        //
                        //     // May be message ID duplication
                        //     PFS__TERMINATE(pos == _assemblers.end(), "Fix meshnet::im_incoming_processor algorithm");
                        //
                        //     auto res = _assemblers.insert({*msgid_opt, multipart_assembler{
                        //         pkt.total_size, pkt.part_size, pkt.initial_sn, pkt.last_sn}});
                        //
                        //     PFS__ASSERT(res.second, "Unexpected error or not enough memory");
                        //
                        //     pos = res.first;
                        //
                        //     auto & ma = pos->second;
                        //     ma.emplace_part(pkt.sn(), std::move(part));
                        //
                        //
                        //     // TODO Process message credentials and first part
                        //     // TODO Send ACK packet
                        // } else if (is_acknowledged(h.sn())) {
                        //     // TODO Send ACK packet
                        // } else {
                        //     // TODO Send NACK packets
                        // }



                        // OLD CODE BELOW


    //                     if (_inproc->payload_expected(h.id())) {
    //                         LOGD(_name.c_str(), "RCV: PAYLOAD: ACK: sid={}", h.id());
    //
    //                         // Send prepared `ack` packet
    //                         _callbacks.dispatch(ack(h.id()));
    //
    //                         // Process payload
    //                         _callbacks.on_payload(std::move(pkt.bytes));
    //
    //                         // Update (increment to next value) committed serial ID
    //                         _inproc->commit(h.id());
    //                     } else if (_inproc->payload_duplicated(h.id())) {
    //                         LOGD(_name.c_str(), "RCV: PAYLOAD: NACK: sid={}", h.id());
    //
    //                         _callbacks.dispatch(nack(h.id()));
    //                     } else {
    //                         LOGD(_name.c_str(), "RCV: PAYLOAD: AGAIN: sid={}", h.id());
    //
    //                         // Previous payloads are lost.
    //                         // Cache current payload.
    //                         _inproc->cache(h.id(), std::move(pkt.bytes));
    //
    //                         // Obtain list of missed serial IDs
    //                         auto missed = _inproc->missed(h.id());
    //                         _callbacks.dispatch(again(missed));
    //                     }

                        break;
                    }

    //                 case packet_enum::report: {
    //                     LOGD(_name.c_str(), "RCV: REPORT");
    //
    //                     report_packet pkt {h, in};
    //
    //                     if (in.commit_transaction())
    //                         _callbacks.on_report(std::move(pkt.bytes));
    //
    //                     break;
    //                 }
    //
    //                 case packet_enum::ack: {
    //                     LOGD(_name.c_str(), "RCV: ACK: sid={}", h.id());
    //
    //                     ack_packet pkt {h, in};
    //
    //                     if (in.commit_transaction())
    //                         _outproc->ack(h.id());
    //
    //                     break;
    //                 }
    //
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

    struct account
    {
        serial_number sn;
        time_point_type exp_time;

        account (serial_number sn, time_point_type exp_time)
            : sn(sn)
            , exp_time(exp_time)
        {}
    };

private:
    // Bounds for sliding window
    //
    // last acknowledged serial ID (ack_sn)
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
    // recent_sn = ack_sn + window_size
    //

    // SYN packet expiration time
    time_point_type _exp_syn;

    // Serial number synchronization flag: set to true when received SYN packet response.
    bool _synchronized {false};

    serial_number _recent_sn {0}; // Serial number of the last message part

    std::uint32_t _part_size {0};   // Message portion size
    std::uint16_t _window_size {1}; // Window size for outgoing message parts

    std::chrono::milliseconds _exp_timeout {3000}; // Expiration timeout

    // Window to track outgoing message/report parts (need access to random element)
    std::deque<account> _window;

    // Queue for outgoing messages/reports (need access to random element)
    std::deque<multipart_tracker> _q;

public:
    im_outgoing_processor (std::uint32_t part_size = 2048U // 32767U // FIXME Small value for test purposes
        , std::uint16_t window_size = 1
        , std::chrono::milliseconds exp_timeout = std::chrono::milliseconds{3000})
        : _part_size(part_size)
        , _window_size(window_size)
        , _exp_timeout(exp_timeout)
    {}

public:
    bool is_synchronized () const noexcept
    {
        return _synchronized;
    }

    void set_synchronized (bool value) noexcept
    {
        _synchronized = value;
    }

    bool syn_expired () const noexcept
    {
        return _exp_syn <= clock_type::now();
    }

    /**
     * Enqueues regular message.
     */
    bool enqueue_message (message_id msgid, int priority, bool force_checksum, std::vector<char> && msg)
    {
        _q.emplace_back(message_id_traits::to_string(msgid), priority, force_checksum
            , _part_size, ++_recent_sn, std::move(msg));
        auto & mt = _q.front();
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
            , _part_size, ++_recent_sn, msg, length);
        auto & mt = _q.front();
        _recent_sn = mt.last_sn();
        return true;
    }

    /**
     * Enqueues report message.
     */
    bool enqueue_report (int priority, bool force_checksum, std::vector<char> && msg)
    {
        _q.emplace_back(priority, force_checksum, _part_size, ++_recent_sn, std::move(msg));
        auto & mt = _q.front();
        _recent_sn = mt.last_sn();
        return true;
    }

    /**
     * Enqueues report message.
     */
    bool enqueue_static_report (int priority, bool force_checksum, char const * msg, std::size_t length)
    {
        _q.emplace_back(priority, force_checksum, _part_size, ++_recent_sn, msg, length);
        auto & mt = _q.front();
        _recent_sn = mt.last_sn();
        return true;
    }

    /**
     * Checks whether no message part to transmit.
     */
    bool empty () const
    {
        return _q.empty() && _window.empty();
    }

    std::vector<char> acquire_syn_packet ()
    {
        _exp_syn = clock_type::now() + _exp_timeout;

        auto out = serializer_traits::make_serializer();
        syn_packet pkt {syn_way_enum::request, _recent_sn + 1};
        pkt.serialize(out);
        return out.take();
    }

    /**
     * Acquires message/report part to send.
     *
     * @note Use in combination with can_acquire().
     */
    std::vector<char> acquire_part (int * priority, bool * force_checksum)
    {
        // First search expired part.
        for (auto const & acc: _window) {
            // Found
            if (acc.exp_time <= clock_type::now()) {
                auto pos = multipart_tracker::find(acc.sn, _q.begin(), _q.end());

                PFS__TERMINATE(pos != _q.end(), "Fix meshnet::im_outgoing_processor algorithm");

                auto out = serializer_traits::make_serializer();
                pos->serialize(out, acc.sn);
                return out.take();
            }
        }

        // No data
        if (_q.empty())
            return std::vector<char>{};

        // Window is full
        if (_window.size() >= _window_size)
            return std::vector<char>{};

        // Acquire new part
        //
        // Search message that contains serial number of the last part from window incrementing
        // by one - next part to transmit.
        serial_number sn = 0;

        if (!_window.empty()) {
            auto const & acc = _window.back();
            sn = acc.sn + 1;
        } else {
            sn = _q.front().initial_sn();
        }

        auto pos = multipart_tracker::find(sn, _q.begin(), _q.end());

        PFS__TERMINATE(pos != _q.end(), "Fix meshnet::im_outgoing_processor algorithm");

        _window.emplace_back(sn, clock_type::now() + _exp_timeout);

        auto out = serializer_traits::make_serializer();
        pos->serialize(out, sn);
        return out.take();
    }

    // // void ack (serial_id sid)
    // // {
    // //     PFS__ASSERT(sid == _ack_sid + 1, "");
    // //     _cache.erase(_cache.begin());
    // //     ++_ack_sid;
    // //     PFS__ASSERT(_recent_sid >= _ack_sid, "");
    // // }
    // //
    // // std::vector<char> payload (serial_id sid)
    // // {
    // //     PFS__ASSERT(_ack_sid < _recent_sid, "");
    // //     PFS__ASSERT(sid <= _recent_sid, "");
    // //     PFS__ASSERT(sid > _ack_sid, "");
    // //
    // //     auto index = sid - _ack_sid - 1;
    // //
    // //     PFS__ASSERT(!_cache[index].payload.empty(), "");
    // //
    // //     return _cache[index].payload;
    // // }
    // //
    // // bool has_waiting () const
    // // {
    // //     return !_cache.empty();
    // // }
    // //
    // // /**
    // //  * @param f Invokable with signature `void (std::vector<char>)`.
    // //  */
    // // template <typename F>
    // // void foreach_waiting (F && f)
    // // {
    // //     if (_cache.empty())
    // //         return;
    // //
    // //     auto now = clock_type::now();
    // //
    // //     if (_oldest_exp_time > now)
    // //         return;
    // //
    // //     for (auto const & acc: _cache) {
    // //         if (acc.exp_time < now) {
    // //             _oldest_exp_time = (std::min)(_oldest_exp_time, acc.exp_time);
    // //             f(acc.payload);
    // //         }
    // //     }
    // // }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
