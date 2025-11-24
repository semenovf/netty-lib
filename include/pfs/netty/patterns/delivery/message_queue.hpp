////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.06.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <pfs/assert.hpp>
#include <array>
#include <cstdint>
#include <queue>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace delivery {

template <typename MessageId, std::size_t PrioritySize = 1>
class message_queue
{
public:
    using message_id = MessageId;

private:
    struct item
    {
        message_id msgid;
        std::vector<char> msg;
        char const * data {nullptr};
        std::size_t size {0};

        item (message_id msgid, std::vector<char> m)
            : msgid(msgid)
            , msg(std::move(m))
        {
            data = msg.data();
            size = msg.size();
        }

        item (message_id msgid, char const * m, std::size_t sz)
            : msgid(msgid)
            , data(m)
            , size(sz)
        {}
    };

    using queue_type = std::queue<item>;

private:
    std::array<queue_type, PrioritySize> _qpool;

public:
    message_queue () = default;

    message_queue (message_queue const &) = delete;
    message_queue (message_queue &&) = default;
    message_queue & operator = (message_queue const &) = delete;
    message_queue & operator = (message_queue &&) = default;
    ~message_queue () = default;

public:
    void push (int priority, message_id msgid, std::vector<char> msg)
    {
        auto & q = _qpool[priority];
        q.emplace(msgid, std::move(msg));
    }

    void push (int priority, message_id msgid, char const * msg, std::size_t size)
    {
        auto & q = _qpool[priority];
        q.emplace(msgid, msg, size);
    }

    void commit (int priority, message_id msgid)
    {
        auto & q = _qpool[priority];

        PFS__THROW_UNEXPECTED(!q.empty(), "Fix delivery::message_queue algorithm");

        auto & x = q.front();

        PFS__THROW_UNEXPECTED(x.msgid == msgid, "Fix delivery::message_queue algorithm");

        q.pop();
    }

};

} // namespace delivery

NETTY__NAMESPACE_END
