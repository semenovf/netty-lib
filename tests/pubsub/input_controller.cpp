////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.11.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../serializer_traits.hpp"
#include "pfs/netty/patterns/pubsub/input_controller.hpp"
#include <vector>

using input_controller_t = netty::pubsub::input_controller<serializer_traits_t>;
using frame_t = netty::pubsub::frame<serializer_traits_t>;

static void pack_payload (archive_t & outp, archive_t & payload)
{
    auto frame_size = frame_t::empty_frame_size() + payload.size();
    frame_t::pack(outp, payload, frame_size);
}

TEST_CASE("data") {
    using data_packet_t = netty::pubsub::data_packet;

    int counter = 0;
    std::vector<char> msg_sample {'H', 'e', 'l', 'l', 'o', ',', 'W', 'o', 'r', 'l', 'd', '!',};

    bool force_checksum = true;
    data_packet_t data_packet {force_checksum};

    archive_t payload;
    serializer_traits_t::serializer_type out {payload};
    data_packet.serialize(out, msg_sample.data(), msg_sample.size());
    data_packet.serialize(out, msg_sample.data(), msg_sample.size());
    data_packet.serialize(out, msg_sample.data(), msg_sample.size());

    input_controller_t ic;

    ic.on_data_ready = [&] (archive_t && msg) {
        auto && c = msg.container();
        CHECK_EQ(c, msg_sample);
        counter++;
    };

    archive_t frames;
    pack_payload(frames, payload);
    ic.process_input(std::move(frames));

    CHECK(payload.empty());
    CHECK_EQ(counter, 3);
}
