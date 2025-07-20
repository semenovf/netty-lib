////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.22 Initial version.
//      2025.05.07 Renamed from priority_input_processor.hpp to priority_input_controller.hpp.
//                 Class `priority_input_processor` renamed to `priority_input_controller`.
//      2025.07.06 Refactored to priority_input_account.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "priority_frame.hpp"
#include <pfs/assert.hpp>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <int PriorityCount>
class priority_input_account
{
private:
    std::vector<char> _in; // Buffer to accumulate raw data
    std::array<std::vector<char>, PriorityCount> _pool;

public:
    void append_chunk (std::vector<char> && chunk)
    {
        _in.insert(_in.end(), chunk.begin(), chunk.end());

        while (priority_frame::parse<PriorityCount>(_pool, _in))
            ;
    }

    char const * data (int priority) const
    {
        PFS__THROW_UNEXPECTED(priority >= 0 && priority < PriorityCount
            , "Fix meshnet::priority_input_account algorithm: bad priority");

        return _pool[priority].data();
    }

    std::size_t size (int priority) const
    {
        PFS__THROW_UNEXPECTED(priority >= 0 && priority < PriorityCount
            , "Fix meshnet::priority_input_account algorithm: bad priority");

        return _pool[priority].size();
    }

    void clear (int priority)
    {
        PFS__THROW_UNEXPECTED(priority >= 0 && priority < PriorityCount
            , "Fix meshnet::priority_input_account algorithm: bad priority");

        return _pool[priority].clear();
    }

    void erase (int priority, std::size_t n)
    {
        PFS__THROW_UNEXPECTED(priority >= 0 && priority < PriorityCount
            , "Fix meshnet::priority_input_account algorithm: bad priority");

        auto & b = _pool[priority];
        b.erase(b.begin(), b.begin() + n);
    }

public: // static
    static constexpr int priority_count ()
    {
        return PriorityCount;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
