////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/namespace.hpp>
#include <pfs/endian.hpp>
#include <pfs/binary_istream.hpp>
#include <pfs/binary_ostream.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {

template <pfs::endian Endianess = pfs::endian::network>
struct serializer_traits
{
    using archive_type = std::vector<char>;
    using serializer_type = pfs::v1::binary_ostream<Endianess>;
    using deserializer_type = pfs::v1::binary_istream<Endianess>;

    template <typename ...Args>
    static serializer_type make_serializer (Args &&... args)
    {
        return serializer_type(std::forward<Args>(args)...);
    }

    template <typename ...Args>
    static deserializer_type make_deserializer (Args &&... args)
    {
        return deserializer_type(std::forward<Args>(args)...);
    }
};

using serializer_traits_t = serializer_traits<pfs::endian::network>;

} // namespace patterns

NETTY__NAMESPACE_END
