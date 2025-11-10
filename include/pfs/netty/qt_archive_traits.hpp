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
#include <pfs/numeric_cast.hpp>
#include <QByteArray>
#include <iterator>

NETTY__NAMESPACE_BEGIN

template <>
inline
QByteArray archive_traits<QByteArray>::make_archive (char const * data, std::size_t length)
{
    return archive_type(data, pfs::numeric_cast<int>(length));
}

template <>
inline
QByteArray archive_traits<QByteArray>::make_archive (iterator first, iterator last)
{
    return archive_type(& *first, std::distance(first, last));
}

template <>
inline
QByteArray archive_traits<QByteArray>::make_archive (const_iterator first, const_iterator last)
{
    return archive_type(& *first, std::distance(first, last));
}

template <>
inline
std::size_t archive_traits<QByteArray>::size (archive_type const & ar)
{
    return static_cast<size_type>(ar.size());
}

NETTY__NAMESPACE_END
