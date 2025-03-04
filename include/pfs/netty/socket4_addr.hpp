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
#include "exports.hpp"
#include "inet4_addr.hpp"
#include "pfs/optional.hpp"
#include "pfs/string_view.hpp"
#include <string>
#include <utility>

namespace netty {

class socket4_addr
{
public:
    inet4_addr    addr;
    std::uint16_t port {0};

public:
    /**
     * Parses IPv4 address from string.
     */
    static NETTY__EXPORT pfs::optional<socket4_addr> parse (char const * s, std::size_t n);

    /**
     * Parses IPv4 address from C-string.
     */
    static NETTY__EXPORT pfs::optional<socket4_addr> parse (char const * s);

    /**
     * Parses IPv4 address from string.
     */
    static NETTY__EXPORT pfs::optional<socket4_addr> parse (std::string const & s);

    /**
     * Parses IPv4 address from string view.
     */
    static NETTY__EXPORT pfs::optional<socket4_addr> parse (pfs::string_view s);
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

inline bool operator < (socket4_addr const & a, socket4_addr const & b)
{
    if (a.addr < b.addr)
        return true;

    if (a.addr == b.addr)
        return a.port < b.port;

    return false;
}

inline bool operator > (socket4_addr const & a, socket4_addr const & b)
{
    if (a.addr > b.addr)
        return true;

    if (a.addr == b.addr)
        return a.port > b.port;

    return false;
}

inline bool operator <= (socket4_addr const & a, socket4_addr const & b)
{
    return (a < b || a == b);
}

inline bool operator >= (socket4_addr const & a, socket4_addr const & b)
{
    return (a > b || a == b);
}

} // namespace netty
