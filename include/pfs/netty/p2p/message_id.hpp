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

struct message_id_traits
{
    using type = std::uint64_t;

    static constexpr type initial ()
    {
        return type{0};
    }

    static constexpr type next (type x)
    {
        return x + 1;
    }

    static constexpr bool eq (type a, type b)
    {
        return a == b;
    }

    static constexpr bool less (type a, type b)
    {
        return a < b;
    }
};

}} // namespace netty::p2p


