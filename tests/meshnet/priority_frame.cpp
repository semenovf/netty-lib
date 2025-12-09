////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.11.19 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../serializer_traits.hpp"
#include "pfs/netty/patterns/meshnet/priority_frame.hpp"

using priority_frame_t = netty::meshnet::priority_frame<1, serializer_traits_t>;

TEST_CASE("basic") {
    int priority = 0;
    char sample_payload[] = "ABC";
    auto payload_size = std::strlen(sample_payload);
    archive_t ar;

    {
        archive_t payload {sample_payload, payload_size};
        auto frame_size = priority_frame_t::empty_frame_size() + payload_size;
        priority_frame_t::pack(priority, ar, payload, frame_size);

        CHECK_EQ(ar.size(), priority_frame_t::empty_frame_size() + payload_size);
        CHECK(payload.empty());

        char const * data = ar.data();

        CHECK_EQ(data[0], priority_frame_t::begin_flag());
        CHECK_EQ(data[1], static_cast<char>(priority));
        CHECK_EQ(data[priority_frame_t::empty_frame_size() + payload_size - 1], priority_frame_t::end_flag());
    }

    {
        std::array<archive_t, 1> pool;
        auto success = priority_frame_t::parse(pool, ar);

        REQUIRE(success);
        REQUIRE_EQ(pool[0].size(), payload_size);
        CHECK(ar.empty());

        auto data = pool[0].data();

        CHECK_EQ(data[0], 'A');
        CHECK_EQ(data[1], 'B');
        CHECK_EQ(data[2], 'C');
    }
}

TEST_CASE("exception") {
    archive_t ar;
    archive_t payload {"ABC", 3};
    auto frame_size = priority_frame_t::empty_frame_size() + 3;

    // Bad priority value
    //          |
    //          v
    priority_frame_t::pack(1, ar, payload, frame_size);

    std::array<archive_t, 1> pool;
    CHECK_THROWS_AS(priority_frame_t::parse(pool, ar), netty::error);
}
