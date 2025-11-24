////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.06.14 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "pfs/netty/patterns/priority_tracker.hpp"

struct distribution
{
    std::array<std::size_t, 7> distrib {7, 6, 5, 4, 3, 2, 1};

    static constexpr std::size_t SIZE = 7;

    constexpr std::size_t operator [] (std::size_t i) const noexcept
    {
        return distrib[i];
    }
};

TEST_CASE("default")
{
    std::array<std::size_t, 7> distribution_sample = {7, 6, 5, 4, 3, 2, 1};

    using priority_tracker_t = netty::priority_tracker<distribution>;

    priority_tracker_t t;

    for (int priority = 0; priority < static_cast<int>(distribution::SIZE); priority++)
        for (int i = 0; i < distribution_sample[priority]; i++)
            CHECK_EQ(t.next(), priority);

    CHECK_EQ(t.current(), 6);
    CHECK_EQ(t.next(), 0);
    CHECK_EQ(t.current(), 0);

    t.skip();
    CHECK_EQ(t.current(), 1);
    t.skip();
    CHECK_EQ(t.current(), 2);
    t.skip();
    CHECK_EQ(t.current(), 3);
    t.skip();
    CHECK_EQ(t.current(), 4);
    t.skip();
    CHECK_EQ(t.current(), 5);
    t.skip();
    CHECK_EQ(t.current(), 6);
    t.skip();
    CHECK_EQ(t.current(), 0);

    for (int priority = 0; priority < static_cast<int>(distribution::SIZE); priority++)
        for (int i = 0; i < distribution_sample[priority]; i++)
            CHECK_EQ(t.next(), priority);
}

TEST_CASE("single priority tracker")
{
    using priority_tracker_t = netty::priority_tracker<netty::single_priority_distribution>;
    priority_tracker_t t;

    for (int i = 0; i < 30; i++)
        CHECK_EQ(t.next(), 0);

    CHECK_EQ(t.current(), 0);
}
