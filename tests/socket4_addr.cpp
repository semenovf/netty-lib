////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// License: see LICENSE file
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.05.12 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "pfs/netty/socket4_addr.hpp"

TEST_CASE("inet4_addr") {
    {
        auto localhosts = netty::socket4_addr::resolve("127.0.0.1:4242");
        REQUIRE_FALSE(localhosts.empty());
        CHECK_EQ(localhosts[0].addr, netty::inet4_addr(netty::inet4_addr::localhost_addr_value));
        CHECK_EQ(localhosts[0].port, 4242);
    }

    {
        auto localhosts = netty::socket4_addr::resolve("localhost:4242");
        REQUIRE_FALSE(localhosts.empty());
        CHECK_EQ(localhosts[0].addr, netty::inet4_addr(netty::inet4_addr::localhost_addr_value));
        CHECK_EQ(localhosts[0].port, 4242);
    }
}

