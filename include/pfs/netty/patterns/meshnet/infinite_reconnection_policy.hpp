////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.06.25 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <chrono>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

class infinite_reconnection_policy
{
    unsigned int _attempts {0};

public:
    infinite_reconnection_policy (bool is_gateway)
    {
        (void)is_gateway;
    }

public:
    bool required () const noexcept
    {
        return true;
    }

    unsigned int attempts () const noexcept
    {
        return _attempts;
    }

    std::chrono::seconds fetch_timeout () noexcept
    {
        _attempts++;

        if (_attempts > 30)
            return std::chrono::seconds{15};

        if (_attempts > 15)
            return std::chrono::seconds{10};

        return std::chrono::seconds{5};
    }

public: // static
    static bool supported () noexcept
    {
        return true;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
