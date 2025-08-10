////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// References:
//
// Changelog:
//      2024.04.12 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "socket4_addr.hpp"
#include <pfs/c++support.hpp>
#include <pfs/universal_id.hpp>
#include <string>

namespace netty {

struct PFS__DEPRECATED host4_addr
{
    pfs::universal_id host_id;
    socket4_addr saddr;
};

inline std::string to_string (host4_addr const & haddr)
{
    return to_string(haddr.host_id) + '@' + to_string(haddr.saddr);
}

inline bool operator == (host4_addr const & a, host4_addr const & b)
{
    return a.host_id == b.host_id && a.saddr == b.saddr;
}

inline bool operator != (host4_addr const & a, host4_addr const & b)
{
    return a.host_id != b.host_id && a.saddr != b.saddr;
}

} // namespace netty
