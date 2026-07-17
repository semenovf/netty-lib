////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// References:
//
// Changelog:
//      2026.07.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "inet4_addr.hpp"
#include <pfs/endian.hpp>
#include <pfs/binary_istream.hpp>
#include <pfs/binary_ostream.hpp>
#include <cstdint>

namespace pfs {

template <endian Endianess, typename Archive>
inline void pack (binary_ostream<Endianess, Archive> & out, netty::inet4_addr const & addr)
{
    out << addr.to_ip4();
}

template <endian Endianess>
inline void unpack (binary_istream<Endianess> & in, netty::inet4_addr & addr)
{
    std::uint32_t a;
    in >> a;
    addr = a;
}

} // namespace pfs

