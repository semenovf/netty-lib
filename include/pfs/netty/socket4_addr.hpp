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

class socket4_addr
{
public:
    inet4_addr    addr;
    std::uint16_t port;

public:
    /**
     * Parses IPv4 address from string.
     */
    static std::pair<bool, socket4_addr> parse (char const * s, std::size_t n);

    /**
     * Parses IPv4 address from string.
     */
    static std::pair<bool, socket4_addr> parse (char const * s);

    /**
     * Parses IPv4 address from string.
     */
    static std::pair<bool, socket4_addr> parse (std::string const & s);
};

inline std::string to_string (socket4_addr const & saddr)
{
    return to_string(saddr.addr) + ':' + std::to_string(saddr.port);
}

inline bool operator == (socket4_addr const & a, socket4_addr const & b)
{
    return a.addr == b.addr && a.port == b.port;
}

inline bool operator != (socket4_addr const & a, socket4_addr const & b)
{
    return a.addr != b.addr || a.port != b.port;
}

} // namespace netty
