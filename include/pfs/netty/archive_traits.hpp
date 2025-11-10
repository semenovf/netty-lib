////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.11.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include <vector>

NETTY__NAMESPACE_BEGIN

template <typename ArchiveType = std::vector<char>>
struct archive_traits
{
    using archive_type = ArchiveType;

    static archive_type make_archive (char const * data, std::size_t length)
    {
        return archive_type(data, data + length);
    }
};

} // namespace patterns

NETTY__NAMESPACE_END
