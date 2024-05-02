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

pfs::optional<socket4_addr> socket4_addr::parse (char const * s, std::size_t n)
{
    auto delim_pos = std::find(s, s + n, ':');

    if (delim_pos == s + n)
        return pfs::nullopt;

    auto res = netty::inet4_addr::parse(s, delim_pos - s);

    if (!res)
        return pfs::nullopt;

    std::error_code ec;
    auto port = pfs::to_integer(delim_pos + 1, s + n
        , std::uint16_t{1024}, std::uint16_t{65535}, ec);

    if (ec)
        return pfs::nullopt;

    return socket4_addr{*res, port};
}

pfs::optional<socket4_addr> socket4_addr::parse (char const * s)
{
    return parse(s, std::char_traits<char>::length(s));
}

pfs::optional<socket4_addr> socket4_addr::parse (std::string const & s)
{
    return parse(s.c_str(), s.size());
}

pfs::optional<socket4_addr> socket4_addr::parse (pfs::string_view s)
{
    return parse(s.data(), s.size());
}

} // namespace netty
