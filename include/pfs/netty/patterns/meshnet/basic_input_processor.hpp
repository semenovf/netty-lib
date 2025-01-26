////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.22 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "protocol.hpp"
#include <pfs/assert.hpp>
#include <pfs/utility.hpp>
#include <pfs/netty/namespace.hpp>
#include <cstring>
#include <unordered_map>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class basic_input_processor
{
    using socket_id = typename Node::socket_id;

    struct account
    {
        socket_id sid;
        std::vector<char> b; // Buffer to accumulate raw data
    };

private:
    Node & _node;
    std::unordered_map<socket_id, account> _accounts;

public:
    basic_input_processor (Node & node)
        : _node(node)
    {}

private:
    account * locate_account (socket_id sid)
    {
        auto pos = _accounts.find(sid);

        if (pos == _accounts.end())
            return nullptr;

        auto & acc = pos->second;

        // Inconsistent data: requested socket ID is not equal to account's ID
        PFS__ASSERT(acc.sid == sid, "Fix the algorithm for basic_input_processor");

        return & acc;
    }

public:
    void add (socket_id sid)
    {
        auto * pacc = locate_account(sid);

        if (pacc != nullptr)
            remove(sid);

        account a;
        a.sid = sid;
        /*auto res = */_accounts.emplace(sid, std::move(a));
    }

    void remove (socket_id sid)
    {
        _accounts.erase(sid);
    }

    void process_input (socket_id sid, std::vector<char> && data)
    {
        if (data.empty())
            return;

        auto * pacc = locate_account(sid);

        if (pacc == nullptr) {
            throw error {
                  pfs::errc::unexpected_error
                , tr::f_("input account not found: #{}, fix the algorithm for basic_input_processor", sid)
            };
            return;
        }

        if (pacc->sid != sid) {
            throw error {
                  pfs::errc::unexpected_data
                , tr::f_("socket IDs is not equal: #{} != #{}, fix the algorithm for basic_input_processor"
                    , pacc->sid, sid)
            };
            return;
        }

        auto & inpb = pacc->b;
        auto offset = inpb.size();
        inpb.resize(offset + data.size());
        std::memcpy(inpb.data() + offset, data.data(), data.size());

        auto in = Node::serializer_traits::make_deserializer(inpb.data(), inpb.size());

        while (in.available() > 0) {
            in.start_transaction();
            header h {in};

            // Not enough data
            if (!in.commit_transaction())
                break;

            switch (h.type()) {
                case packet_enum::handshake: {
                    in.start_transaction();
                    handshake_packet pkt {h, in};

                    if (in.commit_transaction())
                        _node.handshake_processor().process(sid, pkt);

                    break;
                }

                case packet_enum::heartbeat: {
                    in.start_transaction();
                    heartbeat_packet pkt {h, in};

                    if (in.commit_transaction())
                        _node.heartbeat_processor().process(sid, pkt);
                    break;
                }

                case packet_enum::data:
                    break;

                default:
                    throw error {
                          pfs::errc::unexpected_data
                        , tr::f_("unexpected packet type: {}", pfs::to_underlying(h.type()))
                    };
                    break;
            }
        }

        if (in.available() == 0)
            inpb.clear();
        else
            inpb.erase(inpb.begin(), inpb.begin() + (inpb.size() - in.available()));
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
