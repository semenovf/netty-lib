////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.17 Initial version (in_memory_processor.hpp).
//      2025.05.06 `im_incoming_processor` renamed to `incoming_controller`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../error.hpp"
#include "../../namespace.hpp"
#include "../../tag.hpp"
#include "../../trace.hpp"
#include "../priority_tracker.hpp"
#include "multipart_assembler.hpp"
#include "protocol.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <pfs/optional.hpp>
#include <pfs/utility.hpp>
#include <chrono>
#include <cstdint>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

/**
 * Incoming messages controller
 */
template <typename MessageId
    , typename SerializerTraits
    , std::size_t PrioritySize = 1
    , std::uint32_t LostThreshold = 1024> // Max number of lost message parts
class incoming_controller
{
    using message_id = MessageId;
    using serializer_traits = SerializerTraits;

    struct item
    {
        // Expected income message part serial number
        serial_number expected_sn {0};

        // Acknowlaged serial number
        serial_number acked_sn {0};

        pfs::optional<multipart_assembler<message_id>> assembler;
    };

private:
    std::array<item, PrioritySize> _items;

public:
    incoming_controller ()
    {}

private:
    template <typename Manager>
    void process_message_part (Manager * m, typename Manager::address_type sender_addr, int priority
        , header * pkt, std::vector<char> && part)
    {
        auto initial_part = pkt->type() == packet_enum::message;

        auto & x = _items[priority];

        if (initial_part) {
            PFS__THROW_UNEXPECTED(!x.assembler.has_value()
                , tr::f_("Fix delivery::incoming_controller algorithm: priority={}; pkt->sn()={}"
                    , priority, pkt->sn()));
        } else {
            PFS__THROW_UNEXPECTED(x.assembler.has_value()
                , tr::f_("Fix delivery::incoming_controller algorithm: priority={}; pkt->sn()={}"
                    , priority, pkt->sn()));
        }

        // Drop received packet and NAK expected serial number
        if (x.expected_sn != pkt->sn()) {
            if (x.expected_sn < pkt->sn()) {
                auto diff = pkt->sn() - x.expected_sn;

                // Too big number of lost parts (network problem?)
                // NAK only expected serial number
                if (diff > LostThreshold)
                    diff = 0;

                auto out = serializer_traits::make_serializer();

                nak_packet nak_pkt {x.expected_sn, x.expected_sn + diff};
                nak_pkt.serialize(out);
                m->enqueue_private(sender_addr, out.take(), priority);
            } else {
                PFS__THROW_UNEXPECTED(x.expected_sn > pkt->sn()
                    , tr::f_("Fix delivery::incoming_controller algorithm: expected_sn={} > pkt->sn()={}"
                        , x.expected_sn, pkt->sn()));
            }

            return;
        }

        if (initial_part) {
            auto concrete_pkt = static_cast<message_packet<message_id> *>(pkt);
            multipart_assembler<message_id> assembler { concrete_pkt->msgid, concrete_pkt->total_size
                , concrete_pkt->part_size, pkt->sn(), concrete_pkt->last_sn };

            assembler.acknowledge_part(pkt->sn(), std::move(part));
            x.assembler = std::move(assembler);
        } else {
            x.assembler->acknowledge_part(pkt->sn(), std::move(part), true);
        }

        x.expected_sn = pkt->sn() + 1; //x.assembler->last_sn() + 1;

        // Send ACK packet
        auto out = serializer_traits::make_serializer();
        ack_packet ack_pkt {pkt->sn()};
        ack_pkt.serialize(out);
        m->enqueue_private(sender_addr, out.take(), priority);

        if (initial_part) {
            m->process_message_receiving_begin(sender_addr, x.assembler->msgid()
                , x.assembler->total_size());
        }

        m->process_message_receiving_progress(sender_addr, x.assembler->msgid()
            , x.assembler->received_size(), x.assembler->total_size());

        if (x.assembler->is_complete()) {
            m->process_message_received(sender_addr, x.assembler->msgid(), priority
                , x.assembler->payload());

            x.assembler.reset();
        }
    }

public:
    template <typename Manager>
    void process_input (Manager * m, typename Manager::address_type sender_addr, int priority
        , std::vector<char> data)
    {
        auto in = serializer_traits::make_deserializer(data.data(), data.size());
        in.start_transaction();

        // Data can contain more than one packet.
        do {
            header h {in};

            if (in.is_good()) {
                switch (h.type()) {
                    case packet_enum::syn: {
                        syn_packet pkt {h, in};

                        if (pkt.is_request()) {
                            PFS__THROW_UNEXPECTED(pkt.sn_count() == PrioritySize
                                , "Incompatible priority size");

                            // FIXME Need to initialize values from outgoing controller
                            std::vector<serial_number> snumbers;
                            snumbers.reserve(PrioritySize);

                            for (std::size_t i = 0; i < pkt.sn_count(); i++) {
                                auto & x = _items[i];
                                auto acked_sn = pkt.sn_at(i);

                                // Sender totally reloaded/reset
                                if (acked_sn == 0)
                                    x.assembler.reset();

                                x.expected_sn = acked_sn + 1;

                                NETTY__TRACE(TAG, "SYN request received from: {}; priority={}, expected_sn={}"
                                    , to_string(sender_addr), i, _items[i].expected_sn);
                            }

                            // Serial number is no matter for response
                            auto out = serializer_traits::make_serializer();
                            syn_packet response_pkt {syn_way_enum::response, pkt.sn_at(0)};
                            response_pkt.serialize(out);
                            m->enqueue_private(sender_addr, out.take(), 0);
                        } else {
                            NETTY__TRACE(TAG, "SYN response received from: {}; priority={}"
                                , to_string(sender_addr), priority);

                            m->process_ready(sender_addr);
                        }

                        break;
                    }

                    case packet_enum::ack: {
                        ack_packet pkt {h, in};

                        if (!in.commit_transaction())
                            break;

                        // FIXME notify multipart_assembler

                        m->process_acknowledged(sender_addr, priority, pkt.sn());
                        break;
                    }

                    case packet_enum::nak: {
                        nak_packet pkt {h, in};

                        if (!in.commit_transaction())
                            break;

                        m->process_again(sender_addr, priority, pkt.sn(), pkt.last_sn);
                        break;
                    }

                    case packet_enum::message: {
                        std::vector<char> part;
                        message_packet<message_id> pkt {h, in, part};

                        if (!in.commit_transaction())
                            break;

                        process_message_part(m, sender_addr, priority, & pkt, std::move(part));
                        break;
                    }

                    case packet_enum::part: {
                        std::vector<char> part;
                        part_packet pkt {h, in, part};

                        if (!in.commit_transaction())
                            break;

                        process_message_part(m, sender_addr, priority, & pkt, std::move(part));
                        break;
                    }

                    case packet_enum::report: {
                        std::vector<char> bytes;
                        report_packet pkt {h, in, bytes};

                        if (!in.commit_transaction())
                            break;

                        m->process_report_received(sender_addr, priority, std::move(bytes));
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
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
