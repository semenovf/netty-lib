////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.11.19 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include "archive.hpp"
#include <pfs/numeric_cast.hpp>
#include <QByteArray>
#include <cstdint>

NETTY__NAMESPACE_BEGIN

template <>
inline char const * archive<QByteArray>::data (container_type const & c)
{
    return c.data();
}

template <>
inline std::size_t archive<QByteArray>::size (container_type const & c)
{
    return static_cast<std::size_t>(c.size());
}

template <>
inline void archive<QByteArray>::append (container_type & c, char const * data, std::size_t n)
{
    c.append(data, pfs::numeric_cast<qsizetype>(n));
}

template <>
inline void archive<QByteArray>::clear (container_type & c)
{
    c.clear();
}

template <>
inline void archive<QByteArray>::erase (container_type & c, std::size_t pos, std::size_t n)
{
    c.remove(pfs::numeric_cast<qsizetype>(pos), pfs::numeric_cast<qsizetype>(n));
}

template <>
inline void archive<QByteArray>::resize (container_type & c, std::size_t n)
{
    c.resize(pfs::numeric_cast<qsizetype>(n));
}

template <>
inline void archive<QByteArray>::copy (container_type & c, char const * data, std::size_t n
    , std::size_t pos)
{
    std::copy(data, data + n, c.data() + pos);
}

NETTY__NAMESPACE_END
