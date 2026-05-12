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
#include "pfs/netty/socket4_addr.hpp"
#include <pfs/i18n.hpp>
#include <pfs/integer.hpp>
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

std::vector<socket4_addr> socket4_addr::resolve (std::string const & name, error * perr)
{
    auto delim_pos = std::find(name.begin(), name.end(), ':');

    if (delim_pos == name.end()) {
        pfs::throw_or(perr, std::make_error_code(std::errc::invalid_argument)
            , tr::f_("unable to resolve bad socket address: {}", name));
        return std::vector<socket4_addr>{};
    }

    std::error_code ec;
    auto port = pfs::to_integer(delim_pos + 1, name.end()
        , std::uint16_t{1024}, std::uint16_t{65535}, ec);

    if (ec) {
        pfs::throw_or(perr, std::make_error_code(std::errc::invalid_argument)
            , tr::f_("bad port in socket address: {}", name));
        return std::vector<socket4_addr>{};
    }

    error err;
    auto domain_name = std::string(name.begin(), delim_pos);
    std::vector<inet4_addr> addrs = netty::inet4_addr::resolve(domain_name, & err);

    if (err) {
        pfs::throw_or(perr, std::make_error_code(std::errc::invalid_argument)
            , tr::f_("bad socket address: {}", name));
        return std::vector<socket4_addr>{};
    }

    std::vector<socket4_addr> result;
    result.reserve(addrs.size());

    for (auto const & x: addrs)
        result.push_back(socket4_addr{x, port});

    return result;
}

} // namespace netty
