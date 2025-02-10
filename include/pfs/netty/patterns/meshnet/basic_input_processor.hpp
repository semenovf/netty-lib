////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "protocol.hpp"
#include <pfs/assert.hpp>
#include <pfs/utility.hpp>
#include <pfs/netty/error.hpp>
#include <pfs/netty/namespace.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Derived, typename Node>
class basic_input_processor
{
    using socket_id = typename Node::socket_id;

protected:
    Node & _node;

public:
    basic_input_processor (Node & node)
        : _node(node)
    {}

public:
    void process_input (socket_id sid, std::vector<char> && chunk)
    {
        if (chunk.empty())
            return;

        auto that = static_cast<Derived *>(this);
        auto * pacc = that->locate_account(sid);

        PFS__TERMINATE(pacc != nullptr, "account not found, fix");

        that->append_chunk(*pacc, std::move(chunk));

        while (that->read_frame(*pacc)) {
            auto & inpb = that->inpb_ref(*pacc);

            auto in = Node::serializer_traits::make_deserializer(inpb.data(), inpb.size());
            bool has_more_packets = true;

            while (has_more_packets && in.available() > 0) {
                in.start_transaction();
                header h {in};

                // Not enough data for header
                if (!in.commit_transaction())
                    break;

                switch (h.type()) {
                    case packet_enum::handshake: {
                        in.start_transaction();
                        handshake_packet pkt {h, in};

                        if (in.commit_transaction())
                            that->process(sid, pkt);
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::heartbeat: {
                        in.start_transaction();
                        heartbeat_packet pkt {h, in};

                        if (in.commit_transaction())
                            that->process(sid, pkt);
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::data: {
                        in.start_transaction();
                        data_packet pkt {h, in};

                        if (in.commit_transaction())
                            that->process(sid, std::move(pkt.bytes));
                        else
                            has_more_packets = false;

                        break;
                    }

                    default:
                        throw error {
                              pfs::errc::unexpected_data
                            , tr::f_("unexpected packet type: {}", pfs::to_underlying(h.type()))
                        };

                        has_more_packets = false;
                        break;
                }
            }

            if (in.available() == 0)
                inpb.clear();
            else
                inpb.erase(inpb.begin(), inpb.begin() + (inpb.size() - in.available()));
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

