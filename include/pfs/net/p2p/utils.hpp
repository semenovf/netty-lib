////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.29 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef PFS_NET_P2P_TRACE_LEVEL
#   include "pfs/fmt.hpp"
#endif
#include <chrono>
#include <arpa/inet.h> // htonl

namespace pfs {
namespace net {
namespace p2p {

inline std::chrono::milliseconds current_timepoint ()
{
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::steady_clock;

    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch());
}

#ifdef PFS_NET_P2P_TRACE_LEVEL
#   if PFS_NET_P2P_TRACE_LEVEL >= 1
#       define TRACE_1(format, ...) fmt::print("{}: -- TRACE(1): " format "\n", current_timepoint().count(), __VA_ARGS__)
#   endif
#   if PFS_NET_P2P_TRACE_LEVEL >= 2
#       define TRACE_2(format, ...) fmt::print("{}: -- TRACE(2): " format "\n", current_timepoint().count(), __VA_ARGS__)
#   endif
#   if PFS_NET_P2P_TRACE_LEVEL >= 3
#       define TRACE_3(format, ...) fmt::print("{}: -- TRACE(3): " format "\n", current_timepoint().count(), __VA_ARGS__)
#   endif
#else
#   define TRACE_1(format, ...)
#   define TRACE_2(format, ...)
#   define TRACE_3(format, ...)
#endif

}}} // namespace pfs::net::p2p

