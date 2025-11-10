////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.05 Initial version.
//      2025.05.07 Renamed from simple_input_processor.hpp to simple_input_controller.hpp.
//                 Class `simple_input_processor` renamed to `simple_input_controller`.
//      2025.07.06 Refactored to simple_input_account.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "priority_frame.hpp"
#include <cstdint>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename ArchiveType>
class simple_input_account
{
    using archive_type = ArchiveType;

private:
    archive_type _in; // Buffer to accumulate frames
    archive_type _b;

public:
    void append_chunk (archive_type && chunk)
    {
        _in.insert(_in.end(), chunk.begin(), chunk.end());

        while (priority_frame::parse(_b, _in))
            ;
    }

    archive_type const & archive (int /*priority*/) const
    {
        return _b;
    }

    char const * data (int /*priority*/) const noexcept
    {
        return _b.data();
    }

    std::size_t size (int /*priority*/) const noexcept
    {
        return _b.size();
    }

    void clear (int /*priority*/)
    {
        _b.clear();
    }

    void erase (int /*priority*/, std::size_t n)
    {
        _b.erase(_b.begin(), _b.begin() + n);
    }

public: // static
    static constexpr int priority_count ()
    {
        return 1;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
