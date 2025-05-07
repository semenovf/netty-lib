////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.05.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include <functional>

NETTY__NAMESPACE_BEGIN
    template<typename R, typename ...Args>
    using callback_t = std::function<R, Args...>;
NETTY__NAMESPACE_END
