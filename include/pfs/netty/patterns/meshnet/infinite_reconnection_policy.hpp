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
    std::chrono::seconds _timeout {5};
    unsigned int _attempts {0};

public:
    infinite_reconnection_policy (std::chrono::seconds timeout = std::chrono::seconds{5})
        : _timeout(timeout)
    {}

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
        return _timeout;
    }

public: // static
    static bool supported () noexcept
    {
        return true;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
