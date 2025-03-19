////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.10 Initial version.
//      2025.03.19 Refactored NETTY__TRACE macro.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#if NETTY__TRACE_ENABLED
#   include <pfs/log.hpp>

#   if __ANDROID__
#      define NETTY__TRACE(t, f, ...) __android_log_print_helper(ANDROID_LOG_VERBOSE, t, fmt::format(f , ##__VA_ARGS__))
#   else  // __ANDROID__
#       define NETTY__TRACE(t, f, ...) {                                       \
            fmt::print(stdout, "{} [T] {}: " f "\n"                            \
                , stringify_trace_time(), t , ##__VA_ARGS__); fflush(stdout);}
#   endif // !__ANDROID__
#else // NETTY__TRACE_ENABLED
#   define NETTY__TRACE(t, f, ...)
#endif // !NETTY__TRACE_ENABLED
