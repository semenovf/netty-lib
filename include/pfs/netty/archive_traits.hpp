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
#include <cstdint>

NETTY__NAMESPACE_BEGIN

template <typename ArchiveType>
struct archive_traits
{
    using size_type = std::size_t;
    using archive_type = ArchiveType;
    using iterator = typename archive_type::iterator;
    using const_iterator = typename archive_type::const_iterator;

    static archive_type make_archive (char const * data, size_type length);
    static archive_type make_archive (iterator first, iterator last);
    static archive_type make_archive (const_iterator first, const_iterator last);

    static size_type size (archive_type const &);
    static void assign (archive_type & target, archive_type const & source);
    static void assign (archive_type & target, archive_type && source) noexcept;
};

NETTY__NAMESPACE_END
