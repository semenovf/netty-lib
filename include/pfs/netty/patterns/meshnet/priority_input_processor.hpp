////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.22 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "basic_input_processor.hpp"
#include "priority_frame.hpp"
#include "protocol.hpp"
#include <pfs/assert.hpp>
#include <pfs/netty/namespace.hpp>
#include <cstring>
#include <unordered_map>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <int N, typename Node>
class priority_input_processor: public basic_input_processor<priority_input_processor<N, Node>, Node>
{
    using base_class = basic_input_processor<priority_input_processor<N, Node>, Node>;
    using socket_id = typename Node::socket_id;

    friend base_class;

    struct account
    {
        socket_id sid;
        std::array<std::vector<char>, N> priority_buffers; // Buffers to accumulate raw data
        int current_priority {-1};
        std::vector<char> tmp; // Intermediate buffer
    };

private:
    std::unordered_map<socket_id, account> _accounts;

public:
    priority_input_processor (Node & node)
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
        acc.tmp.insert(acc.tmp.end(), chunk.begin(), chunk.end());
    }

    std::vector<char> & inpb_ref (account & acc)
    {
        PFS__TERMINATE(acc.current_priority >= 0, "unexpected current_priority value, fix");
        return acc.priority_buffers[acc.current_priority];
    }

    int priority (account & acc) const noexcept
    {
        return acc.current_priority;
    }

    /**
     * Attempt too read frame.
     *
     * @return @c true if data contains complete frame, or @c false otherwise.
     */
    bool read_frame (account & acc)
    {
        auto opt_frame = priority_frame::parse(acc.tmp.data(), acc.tmp.size());

        // Incomplete frame
        if (!opt_frame)
            return false;

        acc.current_priority = opt_frame->priority();

        PFS__TERMINATE(acc.current_priority >= 0, "unexpected current_priority value, fix");

        auto & inpb = inpb_ref(acc);
        auto first = acc.tmp.data() + priority_frame::header_size();
        auto frame_size = priority_frame::header_size() + opt_frame->payload_size();

        inpb.insert(inpb.end(), first, first + opt_frame->payload_size());
        acc.tmp.erase(acc.tmp.begin(), acc.tmp.begin() + frame_size);

        return true;
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

    void process (socket_id sid, bool is_response, std::vector<std::pair<std::uint64_t, std::uint64_t>> && route)
    {
        this->_node.process_route_received(sid, is_response, std::move(route));
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
