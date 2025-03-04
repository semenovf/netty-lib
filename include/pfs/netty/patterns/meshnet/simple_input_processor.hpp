////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.05 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "basic_input_processor.hpp"
#include "protocol.hpp"
#include "route_info.hpp"
#include <pfs/assert.hpp>
#include <cstdint>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class simple_input_processor: public basic_input_processor<simple_input_processor<Node>, Node>
{
    using base_class = basic_input_processor<simple_input_processor<Node>, Node>;
    using socket_id = typename Node::socket_id;

    friend base_class;

    struct account
    {
        socket_id sid;
        std::vector<char> b; // Buffer to accumulate raw data
    };

private:
    std::unordered_map<socket_id, account> _accounts;

    // Need to support requirements of basic_input_processor when calling read_frame
    bool _frame_ready {false};

public:
    simple_input_processor (Node & node)
        : base_class(node)
    {}

private:
    account * locate_account (socket_id sid)
    {
        auto pos = _accounts.find(sid);

        if (pos == _accounts.end())
            return nullptr;

        auto & acc = pos->second;

        // Inconsistent data: requested socket ID is not equal to account's ID
        PFS__TERMINATE(acc.sid == sid, "socket IDs are not equal, fix");

        return & acc;
    }

    void append_chunk (account & acc, std::vector<char> && chunk)
    {
        acc.b.insert(acc.b.end(), chunk.begin(), chunk.end());
        _frame_ready = true;
    }

    std::vector<char> & inpb_ref (account & acc)
    {
        return acc.b;
    }

    int priority (account &) const noexcept
    {
        return 0;
    }

    bool read_frame (account &)
    {
        // No frames, only unstructured chunks
        auto res = _frame_ready;
        _frame_ready = false;
        return res;
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

    void process (socket_id sid, handshake_packet const & pkt)
    {
        this->_node.handshake_processor().process(sid, pkt);
    }

    void process (socket_id sid, heartbeat_packet const & pkt)
    {
        this->_node.heartbeat_processor().process(sid, pkt);
    }

    void process (socket_id sid, route_packet const & pkt)
    {
        this->_node.process_route_info(sid, pkt.is_response(), pkt.rinfo);
    }

    void process (socket_id sid, int priority, std::vector<char> && bytes)
    {
        this->_node.process_message_received(sid, priority, std::move(bytes));
    }

    void process (socket_id sid
        , int priority
        , std::pair<std::uint64_t, std::uint64_t> sender_id
        , std::pair<std::uint64_t, std::uint64_t> receiver_id
        , std::vector<char> && bytes)
    {
        this->_node.process_message_received(sid, priority, std::move(sender_id), std::move(receiver_id), std::move(bytes));
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

