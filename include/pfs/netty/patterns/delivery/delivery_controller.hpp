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
#include "../../trace.hpp"
#include "multipart_assembler.hpp"
#include "multipart_tracker.hpp"
#include "protocol.hpp"
#include "tag.hpp"
#include <pfs/log.hpp>
#include <pfs/optional.hpp>
#include <pfs/utility.hpp>
#include <chrono>
#include <cstdint>
#include <queue>
#include <vector>

NETTY__NAMESPACE_BEGIN

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
    using serializer_traits_type = SerializerTraits;

    static constexpr std::size_t PRIORITY_COUNT = PriorityTracker::SIZE;

private:
    using address_type = Address;
    using message_id = MessageId;

    using archive_type = typename serializer_traits_type::archive_type;
    using deserializer_type = typename serializer_traits_type::deserializer_type;
    using serializer_type = typename serializer_traits_type::serializer_type;
    using clock_type = std::chrono::steady_clock;
    using time_point_type = clock_type::time_point;
    using multipart_assembler_type = multipart_assembler<message_id, archive_type>;
    using multipart_tracker_type = multipart_tracker<message_id, archive_type>;

    struct multipart_assembler_item
    {
        // Serial number of the last message part of the last message received
        serial_number last_sn {0};

        pfs::optional<multipart_assembler_type> assembler;
    };

    struct multipart_tracker_item
    {
        // Serial number of the last message part of the last message sent
        serial_number last_sn {0};

        // Queue to track of the outgoing messages
        std::queue<multipart_tracker_type> q;
    };

    // Peer synchronization state
    enum class syn_state: std::uint8_t
    {
          initial = 0
        , request_received = 0b0001
        , response_received = 0b0010
        , complete = 0b0011
    };

private:
    address_type _peer_addr;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Incoming specific members
    ////////////////////////////////////////////////////////////////////////////////////////////////
    std::array<multipart_assembler_item, PRIORITY_COUNT> _assemblers;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Outgoing specific members
    ////////////////////////////////////////////////////////////////////////////////////////////////

    // SYN packet expiration time
    time_point_type _exp_syn;

    syn_state _syn_state {syn_state::initial};

    std::uint32_t _part_size {0}; // Message portion size
    std::chrono::milliseconds _exp_timeout {3000}; // Expiration timeout

    priority_tracker_type _priority_tracker;

    std::array<multipart_tracker_item, PRIORITY_COUNT> _trackers;

    bool _paused {false};

public:
    delivery_controller (address_type peer_addr, std::uint32_t part_size
        , std::chrono::milliseconds exp_timeout)
        : _peer_addr(peer_addr)
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
        archive_type ar;
        serializer_type out {ar};
        ack_packet ack_pkt {sn};
        ack_pkt.serialize(out);
        auto success = m->enqueue_private(_peer_addr, std::move(ar), priority);

        if (!success) {
            m->process_error(tr::f_("there is a problem in communication with the"
                " receiver: {} while sending ACK packet (serial number={})"
                ", message delivery paused.", to_string(_peer_addr), sn));
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
                        , to_string(_peer_addr), pkt.count(), PRIORITY_COUNT));
                return;
            }

            for (std::size_t i = 0; i < pkt.count(); i++) {
                auto & t = _trackers.at(i);
                auto lowest_acked = pkt.at(i);

                NETTY__TRACE(DELIVERY_TAG, "SYN request received from: {}; priority={}; msgid={}, lowest_acked_sn={}"
                    , to_string(_peer_addr), i, to_string(lowest_acked.first), lowest_acked.second);

                // lowest_acked_sn == 0 when:
                //      * sender is in initial state (just [re]started)
                // then top most tracker (if exists) must be reset to initial state.
                if (!t.q.empty()) {
                    auto & mt = t.q.front();
                    mt.reset_to(lowest_acked.first, lowest_acked.second);
                }
            }

            // Serial number is no matter for response
            archive_type ar;
            serializer_type out {ar};
            syn_packet<message_id> response_pkt {};
            response_pkt.serialize(out);
            m->enqueue_private(_peer_addr, std::move(ar), 0);

            _syn_state = static_cast<syn_state>(pfs::to_underlying(_syn_state)
                | pfs::to_underlying(syn_state::request_received));
        } else {
            NETTY__TRACE(DELIVERY_TAG, "SYN response received from: {}", to_string(_peer_addr));

            _syn_state = static_cast<syn_state>(pfs::to_underlying(_syn_state)
                | pfs::to_underlying(syn_state::response_received));
        }

        if (_syn_state == syn_state::complete)
            m->process_peer_ready(_peer_addr);
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
            m->process_message_delivered(_peer_addr, msgid);
        }
    }

    template <typename Manager>
    void process_message_part (Manager * m, int priority, header * h, archive_type && part)
    {
        auto heading_part = h->type() == packet_enum::message;

        auto & a = _assemblers.at(priority);
        bool newly_acknowledged = false;

        if (heading_part) {
            auto pkt = static_cast<message_packet<message_id> *>(h);

            if (a.assembler.has_value()) {
                if (a.assembler->msgid() != pkt->msgid) {
                    NETTY__TRACE(DELIVERY_TAG, "message lost from: {}; msgid={}"
                        , to_string(_peer_addr), to_string(a.assembler->msgid()));

                    m->process_message_lost(_peer_addr, a.assembler->msgid());
                } else {
                    PFS__THROW_UNEXPECTED(a.assembler->first_sn() == h->sn()
                        && a.assembler->last_sn() == pkt->last_sn
                        , tr::f_("Fix delivery::delivery_controller algorithm: priority={}; pkt->sn()={}"
                            , priority, h->sn()));
                }

                a.assembler.reset();
            }

            multipart_assembler_type assembler { pkt->msgid, pkt->total_size, pkt->part_size
                , h->sn(), pkt->last_sn };

            a.assembler = std::move(assembler);

            newly_acknowledged = a.assembler->acknowledge_part(h->sn(), part);
            enqueue_ack_packet(m, priority, h->sn());

            if (newly_acknowledged)
                m->process_message_begin(_peer_addr, a.assembler->msgid(), a.assembler->total_size());
        } else {
            // May be message_packet loss before, ignore part.
            // May be old message part received (see multipart_tracker::acquire_next_part), ignore part.
            // Waiting heading part receiving.
            if (!a.assembler.has_value())
                return;

            newly_acknowledged = a.assembler->acknowledge_part(h->sn(), part);
            enqueue_ack_packet(m, priority, h->sn());
        }

        if (newly_acknowledged) {
            m->process_message_progress(_peer_addr, a.assembler->msgid()
                , a.assembler->received_size(), a.assembler->total_size());
        }

        if (a.assembler->is_complete()) {
            m->process_message_received(_peer_addr, a.assembler->msgid(), priority, a.assembler->take_payload());
            a.assembler.reset();
        }
    }

public:
    template <typename Manager>
    void process_input (Manager * m, int priority, archive_type data)
    {
        deserializer_type in {data.data(), data.size()};

        // Data can contain more than one packet.
        do {
            in.start_transaction();
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
                        archive_type part;
                        message_packet<message_id> pkt {h, in, part};

                        if (!in.commit_transaction())
                            break;

                        process_message_part(m, priority, & pkt, std::move(part));
                        break;
                    }

                    case packet_enum::part: {
                        archive_type part;
                        part_packet pkt {h, in, part};

                        if (!in.commit_transaction())
                            break;

                        process_message_part(m, priority, & pkt, std::move(part));
                        break;
                    }

                    case packet_enum::report: {
                        archive_type bytes;
                        report_packet pkt {h, in, bytes};

                        if (!in.commit_transaction())
                            break;

                        m->process_report_received(_peer_addr, priority, std::move(bytes));
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
    bool syn_expired () const noexcept
    {
        return _exp_syn <= clock_type::now();
    }

    archive_type acquire_syn_packet ()
    {
        std::vector<std::pair<message_id, serial_number>> snumbers;
        snumbers.reserve(PRIORITY_COUNT);

        for (auto const & a: _assemblers) {
            if (a.assembler.has_value())
                snumbers.push_back(std::make_pair(a.assembler->msgid(), a.assembler->lowest_acked_sn()));
            else
                snumbers.push_back(std::make_pair(message_id{}, serial_number{0}));
        }

        archive_type ar;
        serializer_type out {ar};
        syn_packet<message_id> pkt {std::move(snumbers)};
        pkt.serialize(out);

        _exp_syn = clock_type::now() + _exp_timeout;

        return ar;
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
        NETTY__TRACE(DELIVERY_TAG, "message sending has been paused with node: {}", to_string(_peer_addr));
    }

    void resume () noexcept
    {
        _paused = false;
        _syn_state = syn_state::initial;
        NETTY__TRACE(DELIVERY_TAG, "message sending has been resumed with node: {}", to_string(_peer_addr));
    }

    /**
     * Enqueues regular message.
     */
    bool enqueue_message (message_id msgid, int priority, archive_type msg)
    {
        auto & t = _trackers.at(priority);
        t.q.emplace(msgid, priority, _part_size, t.last_sn + 1, std::move(msg), _exp_timeout);
        auto & mt = t.q.back();
        t.last_sn = mt.last_sn();

        NETTY__TRACE(DELIVERY_TAG, "message enqueued to: {}; msgid={}; serial numbers range={}-{}"
            , to_string(_peer_addr), to_string(msgid), mt.first_sn(), mt.last_sn());

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

        NETTY__TRACE(DELIVERY_TAG, "message enqueued to: {}; msgid={}; serial numbers range={}-{}"
            , to_string(_peer_addr), to_string(msgid), mt.first_sn(), mt.last_sn());

        return true;
    }

    template <typename Manager>
    unsigned int step (Manager * m)
    {
        unsigned int n = 0;

        // Initiate synchronization if needed.
        // Send SYN packet to synchronize serial numbers.
        if (_syn_state != syn_state::complete) {
            if (syn_expired()) {
                int priority = 0; // High priority
                auto serialized_syn_packet = acquire_syn_packet();
                auto success = m->enqueue_private(_peer_addr, std::move(serialized_syn_packet), priority);

                if (success) {
                    n++;
                } else {
                    m->process_error(tr::f_("there is a problem in communication with the"
                        " receiver: {} while initiating synchronization, message delivery paused."
                            , to_string(_peer_addr)));
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

            archive_type ar;
            serializer_type out {ar};
            auto sn = mt->acquire_next_part(out);

            if (sn > 0) {
                PFS__THROW_UNEXPECTED(mt->priority() == priority
                    , "Fix delivery::delivery_controller algorithm");

                auto success = m->enqueue_private(_peer_addr, std::move(ar), mt->priority());

                if (success) {
                    n++;
                } else {
                    m->process_error(tr::f_("there is a problem in communication with the"
                        " receiver: {} while sending message (priority={}, serial number={})"
                        ", message delivery paused."
                        , to_string(_peer_addr), mt->priority(), sn));
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
    static archive_type serialize_report (char const * data, std::size_t length)
    {
        archive_type ar;
        serializer_type out {ar};
        report_packet pkt;
        pkt.serialize(out, data, length);
        return ar;
    }

    static archive_type serialize_report (archive_type const & data)
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

} // namespace delivery

NETTY__NAMESPACE_END
