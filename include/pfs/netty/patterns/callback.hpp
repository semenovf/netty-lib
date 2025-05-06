////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.05.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#if NETTY__STD_FUNCTION_CALLBACK
#   include <functional.hpp>
NETTY__NAMESPACE_BEGIN
    template<typename R, typename ...Args>
    using callback_t = std::function<R, Args...>;
NETTY__NAMESPACE_END
#else
    // Lightweight alternative of std::function
#   include <pfs/transient_function.hpp>
NETTY__NAMESPACE_BEGIN
    template<typename R, typename ...Args>
    using callback_t = pfs::transient_function<R, Args...>;
NETTY__NAMESPACE_END
#endif
