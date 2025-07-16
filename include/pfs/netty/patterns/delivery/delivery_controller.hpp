////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version (incoming_controller).
//      2025.07.02 Initial version (merge of `incoming_controller` and `outgoing_controller`).
////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../callback.hpp"
#include "../../error.hpp"
#include "../../namespace.hpp"
#include "../../tag.hpp"
#include "../../trace.hpp"
#include "multipart_assembler.hpp"
#include "multipart_tracker.hpp"
#include "protocol.hpp"
#include <pfs/log.hpp>
#include <pfs/utility.hpp>
#include <chrono>
#include <cstdint>
#include <queue>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

template <typename Address
    , typename MessageId
    , typename SerializerTraits
    , typename PriorityTracker
    , std::uint32_t LostThreshold = 512> // Max number of lost message parts
class delivery_controller
{
public:
    using priority_tracker_type = PriorityTracker;

    static constexpr std::size_t PRIORITY_COUNT = PriorityTracker::SIZE;

private:
    using address_type = Address;
    using message_id = MessageId;
    using serializer_traits = SerializerTraits;
    using clock_type = std::chrono::steady_clock;
    using time_point_type = clock_type::time_point;

    struct assembler_item
    {
        // Serial number of the last message part of the last message received
        serial_number last_sn {0};

        pfs::optional<multipart_assembler<message_id>> assembler;
    };

    struct tracker_item
    {
        // Serial number of the last message part of the last message sent
        serial_number last_sn {0};

        // Queue to track of the outgoing messages
        std::queue<multipart_tracker<message_id>> q;
    };

private:
    address_type _addr;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Incoming specific members
    ////////////////////////////////////////////////////////////////////////////////////////////////
    std::array<assembler_item, PRIORITY_COUNT> _assemblers;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Outgoing specific members
    ////////////////////////////////////////////////////////////////////////////////////////////////

    // SYN packet expiration time
    time_point_type _exp_syn;

    // Serial number synchronization flag: set to true when received SYN packet response.
    bool _synchronized {false};

    std::uint32_t _part_size {0}; // Message portion size
    std::chrono::milliseconds _exp_timeout {3000}; // Expiration timeout

    priority_tracker_type _priority_tracker;

    std::array<tracker_item, PRIORITY_COUNT> _trackers;

    bool _paused {false};

private:
    callback_t<void (std::string const &)> _on_error
        = [] (std::string const & msg) { LOGE(TAG, "{}", msg); };

public:
    delivery_controller (address_type addr, std::uint32_t part_size
        , std::chrono::milliseconds exp_timeout)
        : _addr(addr)
        , _part_size(part_size)
        , _exp_timeout(exp_timeout)
    {}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Methods specific for processing incoming data
////////////////////////////////////////////////////////////////////////////////////////////////////

private:
    template <typename Manager>
    void enqueue_ack_packet (Manager * m, int priority, serial_number sn)
    {
        auto out = serializer_traits::make_serializer();
        ack_packet ack_pkt {sn};
        ack_pkt.serialize(out);
        auto success = m->enqueue_private(_addr, out.take(), priority);

        if (!success) {
            m->process_error(tr::f_("there is a problem in communication with the"
                " receiver: {} while sending ACK packet (serial number={})"
                ", message delivery paused.", to_string(_addr), sn));
            pause();
        }
    }

    template <typename Manager>
    void process_input_syn_packet (Manager * m, syn_packet<message_id> const & pkt)
    {
        if (pkt.is_request()) {
            // Incompatible priority size, ignore
            if (pkt.count() != PRIORITY_COUNT) {
                m->process_error(tr::f_("SYN request received from: {}, but priority count ({})"
                    " is incompatible with own settings: {}"
                        , to_string(_addr), pkt.count(), PRIORITY_COUNT));
                return;
            }

            for (std::size_t i = 0; i < pkt.count(); i++) {
                auto & t = _trackers.at(i);
                auto lowest_acked = pkt.at(i);

                NETTY__TRACE(TAG, "SYN request received from: {}; priority={}; msgid={}, lowest_acked_sn={}"
                    , to_string(_addr), i, to_string(lowest_acked.first), lowest_acked.second);

                // lowest_acked_sn == 0 when:
                //      * sender is in initial state (just [re]started)
                // then top most tracker (if exists) must be reset to initial state.
                if (!t.q.empty()) {
                    auto & mt = t.q.front();
                    mt.reset_to(lowest_acked.first, lowest_acked.second);
                }
            }

            // Serial number is no matter for response
            auto out = serializer_traits::make_serializer();
            syn_packet<message_id> response_pkt {};
            response_pkt.serialize(out);
            m->enqueue_private(_addr, out.take(), 0);
        } else {
            NETTY__TRACE(TAG, "SYN response received from: {}", to_string(_addr));
            set_synchronized(true);
            m->process_ready(_addr);
        }
    }

    template <typename Manager>
    void process_input_ack_packet (Manager * m, int priority, ack_packet const & pkt)
    {
        auto & t = _trackers.at(priority);

        if (t.q.empty())
            return;

        auto & mt = t.q.front();
        auto success = mt.acknowledge_part(pkt.sn());

        // Serial number is out of bounds
        if (!success)
            return;

        // The message has been delivered completely
        if (mt.is_complete()) {
            auto msgid = mt.msgid();
            t.q.pop();
            m->process_message_delivered(_addr, msgid);
        }
    }

    template <typename Manager>
    void process_message_part (Manager * m, int priority, header * h, std::vector<char> && part)
    {
        auto heading_part = h->type() == packet_enum::message;

        auto & a = _assemblers.at(priority);

        if (heading_part) {
            ;
        } else {
            // May be message_packet loss before, ignore part.
            // Waiting heading part receiving
            if (!a.assembler.has_value())
                return;
        }

        if (heading_part) {
            auto pkt = static_cast<message_packet<message_id> *>(h);

            if (a.assembler.has_value()) {
                if (a.assembler->msgid() != pkt->msgid) {
                    m->process_message_lost(_addr, a.assembler->msgid());
                } else {
                    PFS__THROW_UNEXPECTED(a.assembler->first_sn() == h->sn()
                        && a.assembler->last_sn() == pkt->last_sn
                        , tr::f_("Fix delivery::delivery_controller algorithm: priority={}; pkt->sn()={}"
                            , priority, h->sn()));
                }

                a.assembler.reset();
            }

            multipart_assembler<message_id> assembler { pkt->msgid, pkt->total_size, pkt->part_size
                , h->sn(), pkt->last_sn };

            assembler.acknowledge_part(h->sn(), part);
            a.assembler = std::move(assembler);
        } else {
            auto success = a.assembler->acknowledge_part(h->sn(), part);

            PFS__THROW_UNEXPECTED(success
                , tr::f_("Fix delivery::delivery_controller algorithm: serial number is out of bounds"
                    "priority={}; pkt->sn()={}", priority, h->sn()));
        }

        enqueue_ack_packet(m, priority, h->sn());

        if (heading_part) {
            m->process_message_receiving_begin(_addr, a.assembler->msgid()
                , a.assembler->total_size());
        }

        m->process_message_receiving_progress(_addr, a.assembler->msgid()
            , a.assembler->received_size(), a.assembler->total_size());

        if (a.assembler->is_complete()) {
            m->process_message_received(_addr, a.assembler->msgid(), priority
                , a.assembler->payload());
            a.assembler.reset();
        }
    }

public:
    template <typename Manager>
    void process_input (Manager * m, int priority, std::vector<char> const & data)
    {
        auto in = serializer_traits::make_deserializer(data.data(), data.size());
        in.start_transaction();

        // Data can contain more than one packet.
        do {
            header h {in};

            if (in.is_good()) {
                switch (h.type()) {
                    case packet_enum::syn: {
                        syn_packet<message_id> pkt {h, in};

                        if (!in.commit_transaction())
                            break;

                        process_input_syn_packet(m, pkt);
                        break;
                    }

                    case packet_enum::ack: {
                        ack_packet pkt {h, in};

                        if (!in.commit_transaction())
                            break;

                        process_input_ack_packet(m, priority, pkt);
                        break;
                    }

                    case packet_enum::message: {
                        std::vector<char> part;
                        message_packet<message_id> pkt {h, in, part};

                        if (!in.commit_transaction())
                            break;

                        process_message_part(m, priority, & pkt, std::move(part));
                        break;
                    }

                    case packet_enum::part: {
                        std::vector<char> part;
                        part_packet pkt {h, in, part};

                        if (!in.commit_transaction())
                            break;

                        process_message_part(m, priority, & pkt, std::move(part));
                        break;
                    }

                    case packet_enum::report: {
                        std::vector<char> bytes;
                        report_packet pkt {h, in, bytes};

                        if (!in.commit_transaction())
                            break;

                        m->process_report_received(_addr, priority, std::move(bytes));
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
                throw error {
                      make_error_code(pfs::errc::unexpected_error)
                    , tr::_("bad or corrupted header for reliable delivery packet")
                };
            }
        } while (in.available() > 0);
    }

////////////////////////////////////////////////////////////////////////////////////////////////////
// Methods specific for processing outgoing data
////////////////////////////////////////////////////////////////////////////////////////////////////

private:
    void set_synchronized (bool value) noexcept
    {
        _synchronized = value;
    }

    bool syn_expired () const noexcept
    {
        return _exp_syn <= clock_type::now();
    }

    std::vector<char> acquire_syn_packet ()
    {
        std::vector<std::pair<message_id, serial_number>> snumbers;
        snumbers.reserve(PRIORITY_COUNT);

        for (auto const & a: _assemblers) {
            if (a.assembler.has_value())
                snumbers.push_back(std::make_pair(a.assembler->msgid(), a.assembler->lowest_acked_sn()));
            else
                snumbers.push_back(std::make_pair(message_id{}, serial_number{0}));
        }

        auto out = serializer_traits::make_serializer();
        syn_packet<message_id> pkt {std::move(snumbers)};
        pkt.serialize(out);

        _exp_syn = clock_type::now() + _exp_timeout;

        return out.take();
    }

    /**
     * Checks whether no messages to transmit.
     */
    bool nothing_transmit () const
    {
        for (auto const & x: _trackers) {
            if (!x.q.empty())
                return false;
        }

        return true;
    }

public:
    bool paused () const noexcept
    {
        return _paused;
    }

    void pause () noexcept
    {
        _paused = true;
        NETTY__TRACE(TAG, "Message sending has been paused to: {}", to_string(_addr));
    }

    void resume () noexcept
    {
        _paused = false;
        set_synchronized(false);
        NETTY__TRACE(TAG, "Message sending has been resumed to: {}", to_string(_addr));
    }

    /**
     * Enqueues regular message.
     */
    bool enqueue_message (message_id msgid, int priority, std::vector<char> msg)
    {
        auto & t = _trackers.at(priority);
        t.q.emplace(msgid, priority, _part_size, t.last_sn + 1, std::move(msg), _exp_timeout);
        auto & mt = t.q.back();
        t.last_sn = mt.last_sn();

        return true;
    }

    /**
     * Enqueues regular message.
     */
    bool enqueue_static_message (message_id msgid, int priority, char const * msg, std::size_t length)
    {
        auto & t = _trackers.at(priority);
        t.q.emplace(msgid, priority, _part_size, t.last_sn + 1, msg, length, _exp_timeout);
        auto & mt = t.q.back();
        t.last_sn = mt.last_sn();
        return true;
    }

    template <typename Manager>
    unsigned int step (Manager * m)
    {
        unsigned int n = 0;

        // Initiate synchronization if needed.
        // Send SYN packet to synchronize serial numbers.
        if (!_synchronized) {
            if (syn_expired()) {
                int priority = 0; // High priority
                auto serialized_syn_packet = acquire_syn_packet();
                auto success = m->enqueue_private(_addr, std::move(serialized_syn_packet), priority);

                if (success) {
                    n++;
                } else {
                    m->process_error(tr::f_("there is a problem in communication with the"
                        " receiver: {} while initiating synchronization, message delivery paused."
                            , to_string(_addr)));
                    pause();
                }
            }

            return n;
        }

        if (nothing_transmit())
            return n;

        auto saved_priority = _priority_tracker.current();
        auto priority = _priority_tracker.current();

        // Try to acquire next part of the current sending message according to priority
        do {
            auto & t = _trackers.at(priority);

            if (t.q.empty()) {
                priority = _priority_tracker.skip();
                continue;
            }

            auto mt = & t.q.front();

            auto out = serializer_traits::make_serializer();
            auto sn = mt->acquire_next_part(out);

            if (sn > 0) {
                PFS__THROW_UNEXPECTED(mt->priority() == priority
                    , "Fix delivery::delivery_controller algorithm");

                auto success = m->enqueue_private(_addr, out.take(), mt->priority());

                if (success) {
                    n++;
                } else {
                    m->process_error(tr::f_("there is a problem in communication with the"
                        " receiver: {} while sending message (priority={}, serial number={})"
                        ", message delivery paused."
                        , to_string(_addr), mt->priority(), sn));
                    pause();
                }

                priority = _priority_tracker.next();
                break;
            } else {
                priority = _priority_tracker.skip();
                continue;
            }
        } while (_priority_tracker.current() != saved_priority);

        return n;
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

template <typename Address
    , typename MessageId
    , typename SerializerTraits
    , typename PriorityTracker
    , std::uint32_t LostThreshold>
constexpr std::size_t delivery_controller<Address, MessageId, SerializerTraits, PriorityTracker
    , LostThreshold>::PRIORITY_COUNT;

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
