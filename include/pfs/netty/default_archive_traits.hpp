////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.11.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "archive_traits.hpp"
#include <vector>

NETTY__NAMESPACE_BEGIN

template <>
inline
std::vector<char> archive_traits<std::vector<char>>::make_archive (char const * data, std::size_t length)
{
    return archive_type(data, data + length);
}

template <>
inline
std::vector<char> archive_traits<std::vector<char>>::make_archive (iterator first, iterator last)
{
    return archive_type(first, last);
}

template <>
inline
std::vector<char> archive_traits<std::vector<char>>::make_archive (const_iterator first, const_iterator last)
{
    return archive_type(first, last);
}

template <>
inline
std::size_t archive_traits<std::vector<char>>::size (archive_type const & ar)
{
    return ar.size();
}

template <>
inline
void archive_traits<std::vector<char>>::assign (archive_type & target, archive_type const & source)
{
    target = source;
}

template <>
inline
void archive_traits<std::vector<char>>::assign (archive_type & target, archive_type && source) noexcept
{
    target = std::move(source);
}

NETTY__NAMESPACE_END
