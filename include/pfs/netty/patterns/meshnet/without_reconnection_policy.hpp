////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <chrono>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

struct without_reconnection_policy
{
    static bool supported () noexcept
    {
        return false;
    }

    bool required () noexcept
    {
        return false;
    }

    std::chrono::seconds fetch_timeout () const noexcept
    {
        return std::chrono::seconds{0};
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
