////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../serializer_traits.hpp"
#include "pfs/netty/patterns/pubsub/frame.hpp"
#include <cstdint>
#include <cstring>
#include <limits>

using frame_t = netty::pubsub::frame<serializer_traits_t>;

TEST_CASE("basic") {
    char sample_payload[] = "ABC";
    auto payload_size = std::strlen(sample_payload);
    archive_t ar;

    {
        archive_t payload {sample_payload, payload_size};
        auto frame_size = frame_t::empty_frame_size() + payload_size;
        frame_t::pack(ar, payload, frame_size);

        CHECK_EQ(ar.size(), frame_t::empty_frame_size() + payload_size);
        CHECK(payload.empty());

        char const * data = ar.data();

        CHECK_EQ(data[0], frame_t::begin_flag());
        CHECK_EQ(data[frame_t::empty_frame_size() + payload_size - 1], frame_t::end_flag());
    }

    {
        archive_t outp;
        auto success = frame_t::parse(outp, ar);

        REQUIRE(success);
        REQUIRE_EQ(outp.size(), payload_size);
        CHECK(ar.empty());

        auto data = outp.data();

        CHECK_EQ(data[0], 'A');
        CHECK_EQ(data[1], 'B');
        CHECK_EQ(data[2], 'C');
    }
}
