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
#include "pfs/netty/patterns/pubsub/protocol.hpp"
#include "pfs/netty/patterns/pubsub/writer_queue.hpp"

using frame_t = netty::pubsub::frame<serializer_traits_t>;
using input_controller_t = netty::pubsub::input_controller<serializer_traits_t>;
using writer_queue_t = netty::pubsub::writer_queue<serializer_traits_t>;
using data_packet_t = netty::pubsub::data_packet;

TEST_CASE("basic") {
    CHECK_EQ(writer_queue_t::priority_count(), 1);

    int counter = 0;

    archive_t payload;
    serializer_traits_t::serializer_type out {payload};
    bool force_checksum = true;
    data_packet_t data_packet {force_checksum};
    data_packet.serialize(out, "ABC", 3);
    data_packet.serialize(out, "DEF", 3);
    data_packet.serialize(out, "JHI", 3);

    writer_queue_t wq;
    wq.enqueue(0, std::move(payload));

    auto serialized_frame = wq.acquire_frame(100);
    input_controller_t ic;

    ic.on_data_ready = [& counter] (archive_t && msg) {
        switch (counter) {
            case 0: CHECK_EQ(msg, archive_t{"ABC", 3}); counter++; break;
            case 1: CHECK_EQ(msg, archive_t{"DEF", 3}); counter++; break;
            case 2: CHECK_EQ(msg, archive_t{"JHI", 3}); counter++; break;
        }
    };

    ic.process_input(std::move(serialized_frame));

    REQUIRE_EQ(counter, 3);
}

