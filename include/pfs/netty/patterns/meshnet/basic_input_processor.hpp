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
    std::unordered_map<socket_id, account> _accounts;

public:
    basic_input_processor () {}

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

    account & ensure_account (socket_id sid)
    {
        auto pacc = locate_account(sid);

        if (pacc == nullptr) {
            account a;
            a.sid = sid;
            auto res = _accounts.emplace(sid, std::move(a));

            pacc = & res.first->second;
        }

        return *pacc;
    }

public:
    void configure () {}

    void remove (socket_id id)
    {
        _accounts.erase(id);
    }

    void process_input (socket_id sid, std::vector<char> && data)
    {
        if (data.empty())
            return;

        auto & acc = ensure_account(sid);

        PFS__ASSERT(acc.sid == sid, "Fix the algorithm for basic_input_processor");

        auto offset = acc.b.size();
        acc.b.resize(offset + data.size());
        std::memcpy(acc.b.data() + offset, data.data(), data.size());

        auto available = acc.b.size();
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
