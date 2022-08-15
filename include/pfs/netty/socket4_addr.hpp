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

namespace netty {

struct socket4_addr
{
    inet4_addr    addr;
    std::uint16_t port;
};

} // namespace netty
