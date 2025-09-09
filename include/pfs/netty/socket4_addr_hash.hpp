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
#include "socket4_addr.hpp"
#include <functional>

namespace std {

template <>
struct hash<netty::socket4_addr>
{
    // See https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
    std::size_t operator () (netty::socket4_addr const & s) const noexcept
    {
        std::uint32_t a = static_cast<std::uint32_t>(s.addr);
        std::uint32_t b = static_cast<std::uint32_t>(s.port);

        std::hash<std::uint32_t> hasher;
        auto result = hasher(a);
        result ^= hasher(b) + 0x9e3779b9 + (result << 6) + (result >> 2);
        return result;
    }
};

} // namespace std
