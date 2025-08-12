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

template <typename Node, typename InputAccount>
class input_controller
{
    using socket_id = typename Node::socket_id;
    using serializer_traits = typename Node::serializer_traits;
    using node_id = typename Node::node_id;
    using account_type = InputAccount;

protected:
    Node * _node {nullptr};
    std::unordered_map<socket_id, account_type> _accounts;

public:
    input_controller (Node * node)
        : _node(node)
    {}

private:
    account_type * locate_account (socket_id sid)
    {
        auto pos = _accounts.find(sid);

        if (pos == _accounts.end())
            return nullptr;

        auto & acc = pos->second;

        return & acc;
    }

    void process (socket_id sid, handshake_packet<node_id> const & pkt)
    {
        _node->handshake_processor().process(sid, pkt);
    }

    void process (socket_id sid, heartbeat_packet const & pkt)
    {
        _node->heartbeat_processor().process(sid, pkt);
    }

    void process (socket_id sid, alive_packet<node_id> const & pkt)
    {
        _node->process_alive_info(sid, pkt.ainfo);
    }

    void process (socket_id sid, unreachable_packet<node_id> const & pkt)
    {
        _node->process_unreachable_info(sid, pkt.uinfo);
    }

    void process (socket_id sid, route_packet<node_id> const & pkt)
    {
        _node->process_route_info(sid, pkt.is_response(), pkt.rinfo);
    }

    void process (socket_id sid, int priority, std::vector<char> && bytes)
    {
        _node->process_message_received(sid, priority, std::move(bytes));
    }

    void process (socket_id sid, int priority, node_id sender_id
        , node_id receiver_id, std::vector<char> && bytes)
    {
        _node->process_message_received(sid, priority, sender_id, receiver_id, std::move(bytes));
    }

    void forward_global_packet (int priority, node_id sender_id, node_id receiver_id
        , std::vector<char> && bytes)
    {
        _node->forward_global_packet(priority, sender_id, receiver_id, std::move(bytes));
    }

public:
    void add (socket_id sid)
    {
        auto * pacc = locate_account(sid);

        if (pacc != nullptr)
            remove(sid);

        account_type a;
        /*auto res = */_accounts.emplace(sid, std::move(a));
    }

    void remove (socket_id sid)
    {
        _accounts.erase(sid);
    }

    void process_input (socket_id sid, std::vector<char> && chunk)
    {
        if (chunk.empty())
            return;

        auto * pacc = locate_account(sid);

        PFS__THROW_UNEXPECTED(pacc != nullptr, "account not found, fix");

        pacc->append_chunk(std::move(chunk));

        for (int priority = 0; priority < InputAccount::priority_count(); priority++) {
            if (pacc->size(priority) == 0)
                continue;

            auto in = serializer_traits::make_deserializer(pacc->data(priority), pacc->size(priority));
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
                            process(sid, pkt);
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::heartbeat: {
                        heartbeat_packet pkt {h, in};

                        if (in.commit_transaction())
                            process(sid, pkt);
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::alive: {
                        alive_packet<node_id> pkt {h, in};

                        if (in.commit_transaction())
                            process(sid, pkt);
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::unreach: {
                        unreachable_packet<node_id> pkt {h, in};

                        if (in.commit_transaction())
                            process(sid, pkt);
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::route: {
                        route_packet<node_id> pkt {h, in};

                        if (in.commit_transaction())
                            process(sid, pkt);
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::ddata: {
                        std::vector<char> bytes_in;
                        ddata_packet pkt {h, in, bytes_in};

                        if (in.commit_transaction())
                            process(sid, priority, std::move(bytes_in));
                        else
                            has_more_packets = false;

                        break;
                    }

                    case packet_enum::gdata: {
                        std::vector<char> bytes_in;
                        gdata_packet<node_id> pkt {h, in, bytes_in};

                        if (in.commit_transaction()) {
                            if (pkt.receiver_id == _node->id()) {
                                process(sid, priority, pkt.sender_id, pkt.receiver_id, std::move(bytes_in));
                            } else {
                                // Need to forward the message if the node is a gateway, or discard
                                // the message otherwise.
                                if (_node->is_gateway()) {
                                    std::vector<char> ar;
                                    auto out = serializer_traits::make_serializer(ar);
                                    pkt.serialize(out, bytes_in.data(), bytes_in.size());
                                    forward_global_packet(priority, pkt.sender_id
                                        , pkt.receiver_id, std::move(ar));
                                }
                            }
                        } else {
                            has_more_packets = false;
                        }

                        break;
                    }

                    default:
                        throw error {
                              make_error_code(pfs::errc::unexpected_error)
                            , tr::f_("unexpected packet type: {}", pfs::to_underlying(h.type()))
                        };

                        has_more_packets = false;
                        break;
                }
            }

            if (in.available() == 0) {
                pacc->clear(priority);
            } else {
                if (pacc->size(priority) > in.available()) {
                    pacc->erase(priority, pacc->size(priority) - in.available());
                }
            }
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
