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
#include "multipart_assembler.hpp"
#include "protocol.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <pfs/utility.hpp>
#include <chrono>
#include <cstdint>
#include <deque>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

/**
 * Incoming messages controller
 */
template <typename MessageId
    , typename SerializerTraits
    , std::uint32_t LostThreshold = 32> // Max number of lost message parts
class incoming_controller
{
    using message_id = MessageId;
    using serializer_traits = SerializerTraits;

private:
    serial_number _expected_sn {0};  // Expected income message part serial number

    std::deque<multipart_assembler<message_id>> _assemblers;

public:
    incoming_controller ()
    {}

public:
    template <typename Manager>
    void process_packet (Manager * m, typename Manager::address_type sender_addr
        , std::vector<char> && data)
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
                            m->enqueue_private(sender_addr, out.take());
                        } else {
                            m->process_ready(sender_addr);
                        }

                        break;
                    }

                    case packet_enum::ack: {
                        ack_packet pkt {h, in};

                        if (!in.commit_transaction())
                            break;

                        m->process_acknowledged(sender_addr, pkt.sn());
                        break;
                    }

                    case packet_enum::nak: {
                        nak_packet pkt {h, in};

                        if (!in.commit_transaction())
                            break;

                        m->process_again(sender_addr, pkt.sn());
                        break;
                    }

                    case packet_enum::message: {
                        std::vector<char> part;
                        message_packet<message_id> pkt {h, in, part};

                        if (!in.commit_transaction())
                            break;

                        // Drop received packet and NAK expected serial number
                        if (_expected_sn != pkt.sn()) {
                            if (_expected_sn < pkt.sn()) {
                                auto diff = pkt.sn() - _expected_sn;

                                // Too big number of lost parts (network problem?)
                                // NAK only expected serial number
                                if (diff > LostThreshold)
                                    diff = 0;

                                auto out = serializer_traits::make_serializer();

                                for (serial_number sn = _expected_sn; sn <= _expected_sn + diff; sn++) {
                                    nak_packet nak_pkt {sn};
                                    nak_pkt.serialize(out);
                                }

                                m->enqueue_private(sender_addr, out.take());
                            } else {
                                PFS__THROW_UNEXPECTED(false, "Fix meshnet::incoming_controller algorithm");
                            }

                            break;
                        }

                        multipart_assembler<message_id> assembler { pkt.msgid, pkt.total_size
                            , pkt.part_size, pkt.sn(), pkt.last_sn };

                        assembler.emplace_part(pkt.sn(), std::move(part));

                        _expected_sn = assembler.last_sn() + 1;

                        // Send ACK packet
                        auto out = serializer_traits::make_serializer();
                        ack_packet ack_pkt {pkt.sn()};
                        ack_pkt.serialize(out);
                        m->enqueue_private(sender_addr, out.take());

                        m->process_message_receiving_progress(sender_addr, assembler.msgid()
                            , assembler.received_size(), assembler.total_size());

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

                        multipart_assembler<message_id> * assembler_ptr = nullptr;

                        for (auto & a: _assemblers) {
                            if (pkt.sn() >= a.first_sn() && pkt.sn() <= a.last_sn()) {
                                assembler_ptr = & a;
                                break;
                            }
                        }

                        if (assembler_ptr != nullptr) {
                            assembler_ptr->emplace_part(pkt.sn(), std::move(part), true);

                            // Send ACK packet
                            auto out = serializer_traits::make_serializer();
                            ack_packet ack_pkt {pkt.sn()};
                            ack_pkt.serialize(out);
                            m->enqueue_private(sender_addr, out.take());

                            m->process_message_receiving_progress(sender_addr, assembler_ptr->msgid()
                                , assembler_ptr->received_size(), assembler_ptr->total_size());
                        }

                        break;
                    }

                    case packet_enum::report: {
                        std::vector<char> bytes;
                        report_packet pkt {h, in, bytes};

                        if (!in.commit_transaction())
                            break;

                        m->process_report_received(sender_addr, std::move(bytes));
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
                    , tr::f_("bad or corrupted header for reliable delivery packet")
                };
            }
        } while (in.available() > 0);
    }

    template <typename Manager>
    unsigned int step (Manager * m, typename Manager::address_type sender_addr)
    {
        unsigned int n = 0;

        // Check message receiving is complete.
        while (!_assemblers.empty()) {
            auto & a = _assemblers.front();

            if (a.is_complete()) {
                m->process_message_received(sender_addr, a.msgid(), a.payload());
                _assemblers.pop_front();
                n++;
            } else {
                break;
            }
        }

        return n;
    }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
