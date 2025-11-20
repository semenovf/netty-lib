////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.11.18 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/serializer_traits.hpp"

#if NETTY__QT_ENABLED
#   include "pfs/netty/qbytearray_archive.hpp"
using serializer_traits_t = netty::serializer_traits<QByteArray>;
using archive_t = serializer_traits_t::archive_type;

PFS__NAMESPACE_BEGIN
template <>
inline void binary_ostream<endian::network, archive_t>::write (archive_t & ar
    , char const * data, std::size_t n)
{
    ar.append(data, n);
}

template <>
inline void append_bytes<archive_t> (archive_t & ar, char const * data, std::size_t n)
{
    ar.append(data, n);
}
PFS__NAMESPACE_END

#else
#   include "pfs/netty/archive.hpp"
using serializer_traits_t = netty::serializer_traits<std::vector<char>>;
using archive_t = serializer_traits_t::archive_type;
#endif
