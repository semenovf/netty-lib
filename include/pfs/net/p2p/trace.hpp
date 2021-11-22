////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.29 Initial version.
//      2021.10.20 Renamed to `trace.hpp`.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef PFS_NET_P2P__TRACE_LEVEL
#   include "pfs/fmt.hpp"
#   include <chrono>
#   include <string>

inline std::string stringify_trace_time ()
{
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::steady_clock;

    std::uint64_t msecs = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();

    int millis = msecs % 1000;
    std::uint64_t seconds = msecs / 1000;
    std::uint64_t hours   = seconds / 3600;
    seconds -= hours * 3600;
    std::uint64_t minutes = seconds / 60;
    seconds -= minutes * 60;

    //std::uint64_t sample = millis
    //    + (seconds + minutes * 60 + hours * 3600) * 1000;

    return fmt::format("{}:{:02}:{:02}.{:03}"
        , hours, minutes, seconds, millis);
}
#endif

#ifdef PFS_NET_P2P__TRACE_LEVEL
#   if PFS_NET_P2P__TRACE_LEVEL >= 1
#       define TRACE_1(format, ...) fmt::print("{}: -- TRACE(1): " format "\n" \
            , stringify_trace_time(), __VA_ARGS__)
            //pfs::net::p2p::current_timepoint().count(), __VA_ARGS__)
#   endif
#   if PFS_NET_P2P__TRACE_LEVEL >= 2
#       define TRACE_2(format, ...) fmt::print("{}: -- TRACE(2): " format "\n" \
            , stringify_trace_time(), __VA_ARGS__)
            //pfs::net::p2p::current_timepoint().count(), __VA_ARGS__)
#   endif
#   if PFS_NET_P2P__TRACE_LEVEL >= 3
#       define TRACE_3(format, ...) fmt::print("{}: -- TRACE(3): " format "\n" \
            , stringify_trace_time(), __VA_ARGS__)
            //pfs::net::p2p::current_timepoint().count(), __VA_ARGS__)
#   endif

#   define TRACE_D(format, ...) fmt::print("{}: -- TRACE(D): {}:{}: " format "\n"\
        , stringify_trace_time()                                                 \
        , __FILE__, __LINE__                                                     \
        , __VA_ARGS__)
            //pfs::net::p2p::current_timepoint().count(), __VA_ARGS__)

#else
#   define TRACE_1(format, ...)
#   define TRACE_2(format, ...)
#   define TRACE_3(format, ...)
#   define TRACE_D(format, ...)
#endif
