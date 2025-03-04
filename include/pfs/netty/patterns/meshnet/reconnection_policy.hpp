////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/namespace.hpp>
#include <chrono>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

struct reconnection_policy
{
    static std::chrono::seconds timeout ()
    {
        return std::chrono::seconds{5};
    }

    static unsigned int attempts ()
    {
        return 5;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END


