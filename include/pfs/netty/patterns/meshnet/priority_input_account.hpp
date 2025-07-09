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
    std::vector<char> _frames; // Buffers to accumulate raw data
    std::array<std::vector<char>, PriorityCount> _pool;
    int _current_priority {-1};

public:
    void append_chunk (std::vector<char> && chunk)
    {
        _frames.insert(_frames.end(), chunk.begin(), chunk.end());
    }

    /**
     * Attempt to read frame.
     *
     * @return @c true if data contains complete frame, or @c false otherwise.
     */
    bool read_frame ()
    {
        // Priority will be updated
        _current_priority = -1;
        return priority_frame::parse<PriorityCount>(_frames, _pool, _current_priority);
    }

    char const * data () const
    {
        PFS__THROW_UNEXPECTED(_current_priority >= 0 && _current_priority < PriorityCount
            , "Fix meshnet::priority_input_account algorithm: bad priority");

        return _pool[_current_priority].data();
    }

    std::size_t size () const
    {
        PFS__THROW_UNEXPECTED(_current_priority >= 0 && _current_priority < PriorityCount
            , "Fix meshnet::priority_input_account algorithm: bad priority");

        return _pool[_current_priority].size();
    }

    void clear ()
    {
        PFS__THROW_UNEXPECTED(_current_priority >= 0 && _current_priority < PriorityCount
            , "Fix meshnet::priority_input_account algorithm: bad priority");

        return _pool[_current_priority].clear();
    }

    void erase (std::size_t n)
    {
        PFS__THROW_UNEXPECTED(_current_priority >= 0 && _current_priority < PriorityCount
            , "Fix meshnet::priority_input_account algorithm: bad priority");

        auto & b = _pool[_current_priority];
        b.erase(b.begin(), b.begin() + n);
    }

    int priority () const
    {
        PFS__THROW_UNEXPECTED(_current_priority >= 0 && _current_priority < PriorityCount
            , "Fix meshnet::priority_input_account algorithm: bad priority");

        return _current_priority;
    }

public: // static
    static constexpr int priority_count ()
    {
        return PriorityCount;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
