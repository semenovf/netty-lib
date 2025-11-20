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
//      2025.11.19 Renamed to input_account and refactored.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "priority_frame.hpp"
#include <pfs/assert.hpp>
#include <array>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

//
// Incapsulates priority buffer pool.
//
template <int PriorityCount, typename SerializerTraits>
class input_account
{
    using serializer_traits_type = SerializerTraits;
    using archive_type = typename serializer_traits_type::archive_type;
    using priority_frame_type = priority_frame<PriorityCount, serializer_traits_type>;

private:
    archive_type _raw; // Buffer to accumulate raw data
    std::array<archive_type, PriorityCount> _pool;

public:
    input_account () = default;
    input_account (input_account const &) = delete;
    input_account (input_account &&) = delete;
    input_account & operator = (input_account const &) = delete;
    input_account & operator = (input_account &&) = delete;

public:
    // Called from input_controller while process input
    void append_chunk (archive_type chunk)
    {
        append(_raw, chunk);

        while (priority_frame_type::template parse<PriorityCount>(_pool, _raw))
            ;
    }

    // FIXME Need ? Or Replace with something else
    // char const * data (int priority) const
    // {
    //     PFS__THROW_UNEXPECTED(priority >= 0 && priority < PriorityCount
    //         , "Fix meshnet::priority_input_account algorithm: bad priority");
    //
    //     return archive_traits_type::data(_pool[priority]);
    // }
    //
    // std::size_t size (int priority) const
    // {
    //     PFS__THROW_UNEXPECTED(priority >= 0 && priority < PriorityCount
    //         , "Fix meshnet::priority_input_account algorithm: bad priority");
    //
    //     return archive_traits_type::size(_pool[priority]);
    // }

    // void clear (int priority)
    // {
    //     PFS__THROW_UNEXPECTED(priority >= 0 && priority < PriorityCount
    //         , "Fix meshnet::priority_input_account algorithm: bad priority");
    //
    //     return archive_traits_type::clear(_pool[priority]);
    // }
    //
    // void erase (int priority, std::size_t n)
    // {
    //     PFS__THROW_UNEXPECTED(priority >= 0 && priority < PriorityCount
    //         , "Fix meshnet::priority_input_account algorithm: bad priority");
    //
    //     auto & b = _pool[priority];
    //     archive_traits_type::erase(b, 0, n);
    // }

public: // static
    static constexpr int priority_count ()
    {
        return PriorityCount;
    }
};

} // namespace meshnet

NETTY__NAMESPACE_END
