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

namespace patterns {
namespace meshnet {

class reconnection_policy
{
    unsigned int _attempts {0};

public:
    bool required () const noexcept
    {
        return _attempts <= 45;
    }

    std::chrono::seconds fetch_timeout () noexcept
    {
        _attempts++;

        if (_attempts > 45)
            return std::chrono::seconds{0};

        if (_attempts > 15)
            return std::chrono::seconds{30};

        if (_attempts > 5)
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


