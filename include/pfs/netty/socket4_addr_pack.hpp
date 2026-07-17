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
#include "socket4_addr.hpp"
#include "inet4_addr_pack.hpp"

namespace pfs {

template <endian Endianess, typename Archive>
inline void pack (binary_ostream<Endianess, Archive> & out, netty::socket4_addr const & saddr)
{
    out << saddr.addr << saddr.port;
}

template <endian Endianess>
inline void unpack (binary_istream<Endianess> & in, netty::socket4_addr & saddr)
{
    in >> saddr.addr >> saddr.port;
}

} // namespace pfs
