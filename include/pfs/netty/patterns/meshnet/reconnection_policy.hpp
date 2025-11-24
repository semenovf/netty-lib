////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <chrono>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

class reconnection_policy
{
    bool _is_gateway {false};
    unsigned int _attempts {0};

public:
    reconnection_policy (bool is_gateway)
        : _is_gateway(is_gateway)
    {}

public:
    bool required () const noexcept
    {
        // Infinite for gateway
        return _is_gateway ? true : _attempts <= 30;
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

} // namespace meshnet

NETTY__NAMESPACE_END
