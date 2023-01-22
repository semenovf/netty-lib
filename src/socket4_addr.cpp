////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// References:
//
// Changelog:
//      2023.01.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/integer.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include <algorithm>
#include <utility>

namespace netty {

std::pair<bool, socket4_addr> socket4_addr::parse (char const * s, std::size_t n)
{
    auto delim_pos = std::find(s, s + n, ':');

    if (delim_pos == s + n)
        return std::make_pair(false, socket4_addr{});

    auto res = netty::inet4_addr::parse(s, delim_pos - s);

    if (!res.first)
        return std::make_pair(false, socket4_addr{});

    std::error_code ec;
    auto port = pfs::to_integer(delim_pos + 1, s + n
        , std::uint16_t{1024}, std::uint16_t{65535}, ec);

    if (ec)
        return std::make_pair(false, socket4_addr{});

    return std::make_pair(true, socket4_addr{res.second, port});
}

std::pair<bool, socket4_addr> socket4_addr::parse (char const * s)
{
    return parse(s, std::char_traits<char>::length(s));
}

std::pair<bool, socket4_addr> socket4_addr::parse (std::string const & s)
{
    return parse(s.c_str(), s.size());
}

} // namespace netty
