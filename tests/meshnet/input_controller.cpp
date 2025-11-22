////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.11.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../serializer_traits.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"
#include "pfs/netty/patterns/meshnet/input_controller.hpp"
#include <pfs/universal_id.hpp>
#include <pfs/universal_id_pack.hpp>

namespace {
constexpr int PRIORITY_COUNT = 2;
}

using namespace netty::meshnet;
using node_id = pfs::universal_id;
using socket_id = netty::posix::tcp_socket::socket_id;
using input_controller_t = netty::meshnet::input_controller<
      PRIORITY_COUNT
    , socket_id
    , node_id
    , serializer_traits_t>;

using priority_frame_t = netty::meshnet::priority_frame<PRIORITY_COUNT, serializer_traits_t>;

namespace {
constexpr socket_id SID = 42;
}

void pack_payload (int priority, archive_t & outp, archive_t & payload)
{
    auto frame_size = priority_frame_t::empty_frame_size() + payload.size();
    priority_frame_t::pack(priority, outp, payload, frame_size);
}

TEST_CASE("handshake") {
    using handshake_packet_t = handshake_packet<node_id>;

    int counter = 0;
    input_controller_t ic;

    ic.add(SID);

    auto id = pfs::generate_uuid();
    bool is_gateway = true;
    bool behind_nat = true;

    handshake_packet_t handshake_rq {id, is_gateway, behind_nat, packet_way_enum::request};

    archive_t payload;
    serializer_traits_t::serializer_type out {payload};
    handshake_rq.serialize(out);
    handshake_rq.serialize(out);
    handshake_rq.serialize(out);

    ic.on_handshake = [&] (socket_id sid, handshake_packet_t && pkt) {
        REQUIRE_EQ(sid, SID);
        CHECK_EQ(pkt.id(), id);
        counter++;
    };

    archive_t frames;
    pack_payload(0, frames, payload);
    ic.process_input(SID, std::move(frames));

    CHECK(payload.empty());
    CHECK_EQ(counter, 3);
}

TEST_CASE("heartbeat") {
    using heartbeat_packet_t = heartbeat_packet;

    int counter = 0;
    input_controller_t ic;

    ic.add(SID);

    std::uint8_t health_data = 42;
    heartbeat_packet_t heartbeat {health_data};

    archive_t payload;
    serializer_traits_t::serializer_type out {payload};
    heartbeat.serialize(out);
    heartbeat.serialize(out);
    heartbeat.serialize(out);

    ic.on_heartbeat = [&] (socket_id sid, heartbeat_packet_t && pkt) {
        REQUIRE_EQ(sid, SID);
        CHECK_EQ(pkt.health_data(), health_data);
        counter++;
    };

    archive_t frames;
    pack_payload(0, frames, payload);
    ic.process_input(SID, std::move(frames));

    CHECK(payload.empty());
    CHECK_EQ(counter, 3);
}

TEST_CASE("alive") {
    using alive_packet_t = alive_packet<node_id>;

    int counter = 0;
    input_controller_t ic;

    ic.add(SID);

    auto id = pfs::generate_uuid();
    alive_info<node_id> ainfo {id};
    alive_packet_t alive {std::move(ainfo)};

    archive_t payload;
    serializer_traits_t::serializer_type out {payload};
    alive.serialize(out);
    alive.serialize(out);
    alive.serialize(out);

    ic.on_alive = [&] (socket_id sid, alive_packet_t && pkt) {
        REQUIRE_EQ(sid, SID);
        CHECK_EQ(pkt.info().id, id);
        counter++;
    };

    archive_t frames;
    pack_payload(0, frames, payload);
    ic.process_input(SID, std::move(frames));

    CHECK(payload.empty());
    CHECK_EQ(counter, 3);
}

TEST_CASE("unreachable") {
    using unreachable_packet_t = unreachable_packet<node_id>;

    int counter = 0;
    input_controller_t ic;

    ic.add(SID);

    auto gw_id = pfs::generate_uuid();
    auto sender_id = pfs::generate_uuid();
    auto receiver_id = pfs::generate_uuid();

    unreachable_info<node_id> uinfo {gw_id, sender_id, receiver_id};
    unreachable_packet_t unreach {std::move(uinfo)};

    archive_t payload;
    serializer_traits_t::serializer_type out {payload};
    unreach.serialize(out);
    unreach.serialize(out);
    unreach.serialize(out);

    ic.on_unreachable = [&] (socket_id sid, unreachable_packet_t && pkt) {
        REQUIRE_EQ(sid, SID);
        CHECK_EQ(pkt.info().gw_id, gw_id);
        CHECK_EQ(pkt.info().sender_id, sender_id);
        CHECK_EQ(pkt.info().receiver_id, receiver_id);
        counter++;
    };

    archive_t frames;
    pack_payload(0, frames, payload);
    ic.process_input(SID, std::move(frames));

    CHECK(payload.empty());
    CHECK_EQ(counter, 3);
}

TEST_CASE("route") {
    using route_packet_t = route_packet<node_id>;

    input_controller_t ic;

    ic.add(SID);

    auto initiator_id = pfs::generate_uuid();
    auto responder_id = pfs::generate_uuid();
    auto gw1_id = pfs::generate_uuid();
    auto gw2_id = pfs::generate_uuid();

    route_info<node_id> rinfo {
          initiator_id
        , responder_id // for response only
        , std::vector<node_id> { gw1_id, gw2_id } // route (gateways)
    };

    // Request
    {
        int counter = 0;
        route_packet_t route_rq {packet_way_enum::request, rinfo};

        archive_t payload;
        serializer_traits_t::serializer_type out {payload};
        route_rq.serialize(out);
        route_rq.serialize(out);
        route_rq.serialize(out);

        ic.on_route = [&] (socket_id sid, route_packet_t && pkt) {
            REQUIRE_EQ(sid, SID);
            CHECK_FALSE(pkt.is_response());
            CHECK_EQ(pkt.info().initiator_id, initiator_id);
            REQUIRE_EQ(pkt.info().route.size(), 2);
            CHECK_EQ(pkt.info().route[0], gw1_id);
            CHECK_EQ(pkt.info().route[1], gw2_id);
            counter++;
        };

        archive_t frames;
        pack_payload(0, frames, payload);
        ic.process_input(SID, std::move(frames));

        CHECK(payload.empty());
        CHECK_EQ(counter, 3);
    }

    // Response
    {
        int counter = 0;
        route_packet_t route_rq {packet_way_enum::response, rinfo};

        archive_t payload;
        serializer_traits_t::serializer_type out {payload};
        route_rq.serialize(out);
        route_rq.serialize(out);
        route_rq.serialize(out);

        ic.on_route = [&] (socket_id sid, route_packet_t && pkt) {
            REQUIRE_EQ(sid, SID);
            CHECK(pkt.is_response());
            CHECK_EQ(pkt.info().initiator_id, initiator_id);
            CHECK_EQ(pkt.info().responder_id, responder_id);
            REQUIRE_EQ(pkt.info().route.size(), 2);
            CHECK_EQ(pkt.info().route[0], gw1_id);
            CHECK_EQ(pkt.info().route[1], gw2_id);
            counter++;
        };

        archive_t frames;
        pack_payload(0, frames, payload);
        ic.process_input(SID, std::move(frames));

        CHECK(payload.empty());
        CHECK_EQ(counter, 3);
    }
}

TEST_CASE("ddata") {
    using ddata_packet_t = ddata_packet;

    int counter = 0;
    input_controller_t ic;

    ic.add(SID);

    std::vector<char> msg_sample {'H', 'e', 'l', 'l', 'o', ',', 'W', 'o', 'r', 'l', 'd', '!',};

    bool force_checksum = true;
    ddata_packet_t ddata {force_checksum};

    archive_t payload;
    serializer_traits_t::serializer_type out {payload};
    ddata.serialize(out, msg_sample.data(), msg_sample.size());
    ddata.serialize(out, msg_sample.data(), msg_sample.size());
    ddata.serialize(out, msg_sample.data(), msg_sample.size());

    ic.on_ddata = [&] (socket_id sid, int priority, archive_t && msg) {
        REQUIRE_EQ(sid, SID);
        CHECK_EQ(priority, 1);

        auto && c = msg.container();
        CHECK_EQ(c, msg_sample);
        counter++;
    };

    archive_t frames;
    pack_payload(1, frames, payload);
    ic.process_input(SID, std::move(frames));

    CHECK(payload.empty());
    CHECK_EQ(counter, 3);
}

TEST_CASE("gdata") {
    using gdata_packet_t = gdata_packet<node_id>;

    int counter = 0;
    input_controller_t ic;

    ic.add(SID);

    std::vector<char> msg_sample {'H', 'e', 'l', 'l', 'o', ',', 'W', 'o', 'r', 'l', 'd', '!',};

    auto sender_id = pfs::generate_uuid();
    auto receiver_id = pfs::generate_uuid();
    bool force_checksum = true;
    gdata_packet_t ddata {sender_id, receiver_id, force_checksum};

    archive_t payload;
    serializer_traits_t::serializer_type out {payload};
    ddata.serialize(out, msg_sample.data(), msg_sample.size());
    ddata.serialize(out, msg_sample.data(), msg_sample.size());
    ddata.serialize(out, msg_sample.data(), msg_sample.size());

    ic.on_gdata = [&] (socket_id sid, int priority, gdata_packet_t && pkt, archive_t && msg) {
        REQUIRE_EQ(sid, SID);
        CHECK_EQ(priority, 1);
        CHECK_EQ(pkt.sender_id(), sender_id);
        CHECK_EQ(pkt.receiver_id(), receiver_id);

        auto && c = msg.container();
        CHECK_EQ(c, msg_sample);
        counter++;
    };

    archive_t frames;
    pack_payload(1, frames, payload);
    ic.process_input(SID, std::move(frames));

    CHECK(payload.empty());
    CHECK_EQ(counter, 3);
}
