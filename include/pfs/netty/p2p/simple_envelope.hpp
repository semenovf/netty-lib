////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.04.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cstdint>

namespace netty {
namespace p2p {

struct simple_envelope_traits
{
    using id = std::uint64_t;

    static constexpr id initial ()
    {
        return id{0};
    }

    static constexpr id next (id x)
    {
        return x + 1;
    }

    static constexpr bool eq (id a, id b)
    {
        return a == b;
    }

    static constexpr bool less (id a, id b)
    {
        return a < b;
    }
};

}} // namespace netty::p2p


