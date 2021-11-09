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

#ifdef PFS_NET__P2P_TRACE_LEVEL
#   include "timing.hpp"
#   include "pfs/fmt.hpp"
#endif

#ifdef PFS_NET__P2P_TRACE_LEVEL
#   if PFS_NET__P2P_TRACE_LEVEL >= 1
#       define TRACE_1(format, ...) fmt::print("{}: -- TRACE(1): " format "\n" \
            , pfs::net::p2p::to_string(pfs::net::p2p::current_timepoint()), __VA_ARGS__)
            //pfs::net::p2p::current_timepoint().count(), __VA_ARGS__)
#   endif
#   if PFS_NET__P2P_TRACE_LEVEL >= 2
#       define TRACE_2(format, ...) fmt::print("{}: -- TRACE(2): " format "\n" \
            , pfs::net::p2p::to_string(pfs::net::p2p::current_timepoint()), __VA_ARGS__)
            //pfs::net::p2p::current_timepoint().count(), __VA_ARGS__)
#   endif
#   if PFS_NET__P2P_TRACE_LEVEL >= 3
#       define TRACE_3(format, ...) fmt::print("{}: -- TRACE(3): " format "\n" \
            , pfs::net::p2p::to_string(pfs::net::p2p::current_timepoint()), __VA_ARGS__)
            //pfs::net::p2p::current_timepoint().count(), __VA_ARGS__)
#   endif

#   define TRACE_D(format, ...) fmt::print("{}: -- TRACE(D): {}:{}: " format "\n"\
        , pfs::net::p2p::to_string(pfs::net::p2p::current_timepoint())           \
        , __FILE__, __LINE__                                                     \
        , __VA_ARGS__)
            //pfs::net::p2p::current_timepoint().count(), __VA_ARGS__)

#else
#   define TRACE_1(format, ...)
#   define TRACE_2(format, ...)
#   define TRACE_3(format, ...)
#   define TRACE_D(format, ...)
#endif
