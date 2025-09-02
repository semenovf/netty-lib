////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.04 Initial version.
//      2025.08.19 Added constructor.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <chrono>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

class without_reconnection_policy
{
public:
    without_reconnection_policy (bool is_gateway)
    {
        (void)is_gateway;
    }

public:
    bool required () noexcept
    {
        return false;
    }

    unsigned int attempts () const noexcept
    {
        return 0;
    }

    std::chrono::seconds fetch_timeout () const noexcept
    {
        return std::chrono::seconds{0};
    }

public:
    static bool supported () noexcept
    {
        return false;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
