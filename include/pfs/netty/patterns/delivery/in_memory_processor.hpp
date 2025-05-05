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
#include <pfs/utility.hpp>
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

private:
    // serial_number _committed_sn {0}; // Last serial number of the last committed income message
    serial_number _expected_sn {0};  // Expected income message part serial number

    std::deque<multipart_assembler> _assemblers;

public:
    im_incoming_processor ()
    {}

public:
    template <typename OnSend, typename OnReady, typename OnAcknowledged, typename OnAgain
        , typename OnReportReceived>
    void process_packet (std::vector<char> && data, OnSend && on_send, OnReady && on_ready
        , OnAcknowledged && on_acknowledged, OnAgain && on_again
        , OnReportReceived && on_report_received)
    {
        auto in = serializer_traits::make_deserializer(data.data(), data.size());
        in.start_transaction();

        // Data can contains more than one packet.
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

                    case packet_enum::ack: {
                        ack_packet pkt {h, in};

                        if (!in.commit_transaction())
                            break;

                        on_acknowledged(pkt.sn());
                        break;
                    }

                    case packet_enum::nak: {
                        nak_packet pkt {h, in};

                        if (!in.commit_transaction())
                            break;

                        on_again(pkt.sn());
                        break;
                    }

                    case packet_enum::message: {
                        std::vector<char> part;
                        message_packet pkt {h, in, part};

                        if (!in.commit_transaction())
                            break;

                        // Drop received packet and NAK expected serial number
                        if (_expected_sn != pkt.sn()) {
                            auto out = serializer_traits::make_serializer();
                            nak_packet nak_pkt {_expected_sn};
                            nak_pkt.serialize(out);
                            on_send(out.take());
                            break;
                        }

                        PFS__TERMINATE(!pkt.msgid.empty(), "Fix meshnet::im_incoming_processor algorithm");

                        auto msgid_opt = message_id_traits::parse(pkt.msgid);

                        // Bad message ID received
                        if (!msgid_opt)
                            break;

                        multipart_assembler assembler { std::move(pkt.msgid), pkt.total_size
                            , pkt.part_size, pkt.sn(), pkt.last_sn };

                        assembler.emplace_part(pkt.sn(), std::move(part));

                        _expected_sn = assembler.last_sn() + 1;

                        // Send ACK packet
                        auto out = serializer_traits::make_serializer();
                        ack_packet ack_pkt {pkt.sn()};
                        ack_pkt.serialize(out);
                        on_send(out.take());

                        _assemblers.push_back(std::move(assembler));

                        break;
                    }

                    case packet_enum::part: {
                        std::vector<char> part;
                        part_packet pkt {h, in, part};

                        if (!in.commit_transaction())
                            break;

                        // No any message expected, drop part, it can be received later.
                        if (_assemblers.empty())
                            break;

                        multipart_assembler * assembler_ptr = nullptr;

                        for (auto & a: _assemblers) {
                            if (pkt.sn() >= a.first_sn() && pkt.sn() <= a.last_sn()) {
                                assembler_ptr = & a;
                                break;
                            }
                        }

                        if (assembler_ptr != nullptr)
                            assembler_ptr->emplace_part(pkt.sn(), std::move(part), true);

                        break;
                    }

                    case packet_enum::report: {
                        std::vector<char> bytes;
                        report_packet pkt {h, in, bytes};

                        if (!in.commit_transaction())
                            break;

                        on_report_received(std::move(bytes));
                        break;
                    }

                    default:
                        throw error {
                              make_error_code(pfs::errc::unexpected_error)
                            , tr::f_("unexpected packet type: {}", pfs::to_underlying(h.type()))
                        };

                        break;
                }
            }

            if (!in.is_good()) {
                throw error {tr::f_("bad or corrupted header for reliable delivery packet")};
            }
        } while (in.available() > 0);
    }

    template <typename OnMessageReceived>
    unsigned int step (OnMessageReceived && on_message_received)
    {
        unsigned int n = 0;

        // Check message receiving is complete.
        while (!_assemblers.empty()) {
            auto & a = _assemblers.front();

            if (a.is_complete()) {
                auto msgid_opt = message_id_traits::parse(a.msgid());

                // Bad message ID received
                if (!msgid_opt) {
                    throw error {
                          make_error_code(pfs::errc::unexpected_error)
                        , tr::f_("bad message ID received")
                    };
                }

                on_message_received(*msgid_opt, a.payload());
                _assemblers.pop_front();
                n++;
            } else {
                break;
            }
        }

        return n;
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
    // SYN packet expiration time
    time_point_type _exp_syn;

    // Serial number synchronization flag: set to true when received SYN packet response.
    bool _synchronized {false};

    serial_number _recent_sn {0}; // Serial number of the last message part (enqueued)
    std::uint32_t _part_size {0}; // Message portion size
    std::chrono::milliseconds _exp_timeout {3000}; // Expiration timeout

    // Window/queue to track outgoing message/report parts (need access to random element)
    std::deque<multipart_tracker> _q;

    // The queue stores serial numbers for retransmission.
    std::queue<serial_number> _again;

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

        // Retransmit
        if (!_again.empty()) {
            while (!_again.empty()) {
                serial_number sn = _again.front();
                _again.pop();

                auto pos = multipart_tracker::find(_q.begin(), _q.end(), sn);
                PFS__TERMINATE(pos != _q.end(), "Fix meshnet::im_outgoing_processor algorithm");

                auto out = serializer_traits::make_serializer();

                if (pos->acquire_part(out)) {
                    on_send(pos->priority(), pos->force_checksum(), out.take());
                    n++;
                }
            }

            return n;
        }

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

    void acknowledge (serial_number sn)
    {
        auto pos = multipart_tracker::find(_q.begin(), _q.end(), sn);
        PFS__TERMINATE(pos != _q.end(), "Fix meshnet::im_outgoing_processor algorithm");
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
