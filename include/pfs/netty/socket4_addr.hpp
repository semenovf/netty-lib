////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// References:
//
// Changelog:
//      2022.08.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "inet4_addr.hpp"
#include <string>

namespace netty {

struct socket4_addr
{
    inet4_addr    addr;
    std::uint16_t port;
};

inline std::string to_string (socket4_addr const & saddr)
{
    return to_string(saddr.addr) + ':' + std::to_string(saddr.port);
}

} // namespace netty
