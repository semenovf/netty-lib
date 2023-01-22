////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021-2023 Vladislav Trifochkin
//
// License: see LICENSE file
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "pfs/netty/inet4_addr.hpp"

TEST_CASE("inet4_addr") {
    netty::inet4_addr unicast {192, 168, 1, 1};
    netty::inet4_addr multicast_low {224, 0, 0, 0};
    netty::inet4_addr multicast_hi {239, 255, 255, 255};
    netty::inet4_addr broadcast_global {255, 255, 255, 255};
    netty::inet4_addr broadcast {192, 168, 1, 255};

    CHECK_FALSE(netty::is_broadcast(unicast));
    CHECK_FALSE(netty::is_broadcast(multicast_low));
    CHECK_FALSE(netty::is_broadcast(multicast_hi));
    CHECK(netty::is_broadcast(broadcast_global));
    CHECK(netty::is_broadcast(broadcast));

    CHECK_FALSE(netty::is_multicast(unicast));
    CHECK(netty::is_multicast(multicast_low));
    CHECK(netty::is_multicast(multicast_hi));
    CHECK_FALSE(netty::is_multicast(broadcast_global));
    CHECK_FALSE(netty::is_multicast(broadcast));
}
