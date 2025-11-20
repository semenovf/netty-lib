////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.17 Initial version.
//      2025.11.19 Replaced by new version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include "archive.hpp"
#include <pfs/endian.hpp>
#include <pfs/binary_istream.hpp>
#include <pfs/binary_ostream.hpp>
#include <vector>

NETTY__NAMESPACE_BEGIN

template <typename Container = std::vector<char>
    , typename Serializer = pfs::binary_ostream<pfs::endian::network, archive<Container>>
    , typename Deserializer = pfs::binary_istream<pfs::endian::network>>
struct serializer_traits
{
    using container_type = Container;
    using archive_type = archive<container_type>;
    using serializer_type = Serializer;
    using deserializer_type = Deserializer;
};

using default_serializer_traits_t = serializer_traits<>;

NETTY__NAMESPACE_END

PFS__NAMESPACE_BEGIN
template <>
inline void
binary_ostream<endian::network, netty::archive<std::vector<char>>>::write (netty::archive<std::vector<char>> & ar
    , char const * data, std::size_t n)
{
    ar.append(data, n);
}

template <>
inline void
append_bytes<netty::archive<std::vector<char>>> (netty::archive<std::vector<char>> & ar
    , char const * data, std::size_t n)
{
    ar.append(data, n);
}
PFS__NAMESPACE_END
