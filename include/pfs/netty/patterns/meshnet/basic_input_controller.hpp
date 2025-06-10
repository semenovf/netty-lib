////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.05 Initial version.
//      2025.05.07 Renamed from basic_input_processor.hpp to basic_input_controller.hpp.
//                 Class `basic_input_processor` renamed to `basic_input_controller`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../error.hpp"
#include "../../namespace.hpp"
#include "protocol.hpp"
#include <pfs/assert.hpp>
#include <pfs/utility.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Derived, typename Node>
class basic_input_controller
{
    using socket_id = typename Node::socket_id;
    using serializer_traits = typename Node::serializer_traits;
    using node_id = typename Node::node_id;

protected:
    Node * _node {nullptr};

public:
    basic_input_controller (Node * node)
        : _node(node)
    {}

public:
    void process_input (socket_id sid, std::vector<char> && chunk)
    {
        if (chunk.empty())
            return;

        auto that = static_cast<Derived *>(this);
        auto * pacc = that->locate_account(sid);

        PFS__THROW_UNEXPECTED(pacc != nullptr, "account not found, fix");

        that->append_chunk(*pacc, std::move(chunk));

        while (that->read_frame(*pacc)) {
            auto & inpb = that->inpb_ref(*pacc);
            auto priority = that->priority(*pacc);

            PFS__THROW_UNEXPECTED(priority >= 0, "invalid priority, fix");

            auto in = serializer_traits::make_deserializer(inpb.data(), inpb.size());
            bool has_more_packets = true;

            while (has_more_packets && in.available() > 0) {
                in.start_transaction();
                header h {in};

                // Incomplete header
                if (!in.is_good()) {
                    has_more_packets = false;
                    continue;
                }

                switch (h.type()) {
                    case packet_enum::handshake: {
                        handshake_packet<node_id> pkt {h, in};

                        if (in.commit_transaction())
                            that->process(sid, pkt);
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::heartbeat: {
                        heartbeat_packet pkt {h, in};

                        if (in.commit_transaction())
                            that->process(sid, pkt);
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::alive: {
                        alive_packet<node_id> pkt {h, in};

                        if (in.commit_transaction())
                            that->process(sid, pkt);
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::unreach: {
                        unreachable_packet<node_id> pkt {h, in};

                        if (in.commit_transaction())
                            that->process(sid, pkt);
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::route: {
                        route_packet<node_id> pkt {h, in};

                        if (in.commit_transaction())
                            that->process(sid, pkt);
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::ddata: {
                        ddata_packet pkt {h, in};

                        if (in.commit_transaction())
                            that->process_message_received(sid, priority, std::move(pkt.bytes));
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::gdata: {
                        gdata_packet<node_id> pkt {h, in};

                        if (in.commit_transaction()) {
                            if (pkt.receiver_id == _node->id()) {
                                that->process_message_received(sid, priority
                                    , pkt.sender_id
                                    , pkt.receiver_id
                                    , std::move(pkt.bytes));
                            } else {
                                // Need to forward the message if the node is a gateway, or discard
                                // the message otherwise.
                                if (_node->is_gateway()) {
                                    auto out = serializer_traits::make_serializer();
                                    pkt.serialize(out);
                                    that->forward_global_packet(priority, pkt.sender_id
                                        , pkt.receiver_id, out.take());
                                }
                            }
                        } else {
                            has_more_packets = false;
                        }

                        break;
                    }

                    default:
                        throw error {
                            tr::f_("unexpected packet type: {}", pfs::to_underlying(h.type()))
                        };

                        has_more_packets = false;
                        break;
                }
            }

            if (in.available() == 0) {
                inpb.clear();
            } else {
                if (inpb.size() > in.available())
                    inpb.erase(inpb.begin(), inpb.begin() + (inpb.size() - in.available()));
            }
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

