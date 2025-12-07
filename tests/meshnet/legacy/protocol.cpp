////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.12 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../doctest.h"
#include "../../serializer_traits.hpp"
#include "pfs/netty/patterns/meshnet/protocol.hpp"
#include <pfs/universal_id.hpp>
#include <pfs/universal_id_pack.hpp>

using namespace netty::meshnet;
using node_id = pfs::universal_id;

TEST_CASE("handshake_packet") {
    using handshake_packet_t = handshake_packet<node_id>;

    auto id_sample = pfs::generate_uuid();

    bool is_gateway = true;
    bool behind_nat = true;

    handshake_packet_t req_hp {id_sample, is_gateway, behind_nat, packet_way_enum::request};

    CHECK_EQ(req_hp.version(), header::VERSION);

    CHECK_EQ(req_hp.type(), packet_enum::handshake);
    CHECK_FALSE(req_hp.is_response());
    CHECK_FALSE(req_hp.has_checksum());
    CHECK(req_hp.is_gateway());
    CHECK(req_hp.behind_nat());

    handshake_packet_t rep_hp {id_sample, !is_gateway, !behind_nat, packet_way_enum::response};

    CHECK_EQ(rep_hp.type(), packet_enum::handshake);
    CHECK(rep_hp.is_response());
    CHECK_FALSE(rep_hp.has_checksum());
    CHECK_FALSE(rep_hp.is_gateway());
    CHECK_FALSE(rep_hp.behind_nat());

    // Serialization/Deserialization
    {
        archive_t ar;
        serializer_traits_t::serializer_type out {ar};
        req_hp.serialize(out);

        serializer_traits_t::deserializer_type in {ar.data(), ar.size()};
        header h {in};
        handshake_packet_t req_hp1 {h, in};

        CHECK_EQ(req_hp1.version(), header::VERSION);
        CHECK_EQ(req_hp1.type(), packet_enum::handshake);
        CHECK_FALSE(req_hp1.is_response());
        CHECK_FALSE(req_hp1.has_checksum());
        CHECK(req_hp1.is_gateway());
        CHECK(req_hp1.behind_nat());
        CHECK_EQ(req_hp1.id(), id_sample);
    }

    {
        archive_t ar;
        serializer_traits_t::serializer_type out {ar};
        rep_hp.serialize(out);

        serializer_traits_t::deserializer_type in {ar.data(), ar.size()};
        header h {in};
        handshake_packet_t rep_hp1 {h, in};

        CHECK_EQ(rep_hp1.version(), header::VERSION);
        CHECK_EQ(rep_hp1.type(), packet_enum::handshake);
        CHECK(rep_hp1.is_response());
        CHECK_FALSE(rep_hp1.has_checksum());
        CHECK_FALSE(rep_hp1.is_gateway());
        CHECK_FALSE(rep_hp1.behind_nat());
        CHECK_EQ(rep_hp1.id(), id_sample);
    }
}

TEST_CASE("heartbeat_packet") {
    using heartbeat_packet_t = heartbeat_packet;

    std::uint8_t health_data = 42;
    heartbeat_packet_t hbp {health_data};

    CHECK_EQ(hbp.version(), header::VERSION);

    CHECK_EQ(hbp.type(), packet_enum::heartbeat);
    CHECK_FALSE(hbp.has_checksum());

    // Serialization/Deserialization
    archive_t ar;
    serializer_traits_t::serializer_type out {ar};
    hbp.serialize(out);

    serializer_traits_t::deserializer_type in {ar.data(), ar.size()};
    header h {in};
    heartbeat_packet_t hbp1 {h, in};

    CHECK_EQ(hbp1.version(), header::VERSION);
    CHECK_EQ(hbp1.type(), packet_enum::heartbeat);
    CHECK_FALSE(hbp1.has_checksum());
    CHECK_EQ(hbp1.health_data(), health_data);
}

TEST_CASE("alive_packet") {
    using alive_packet_t = alive_packet<node_id>;

    alive_info<node_id> ainfo_sample { pfs::generate_uuid() };
    alive_packet_t ap {ainfo_sample};

    CHECK_EQ(ap.version(), header::VERSION);

    CHECK_EQ(ap.type(), packet_enum::alive);
    CHECK_FALSE(ap.has_checksum());

    // Serialization/Deserialization
    archive_t ar;
    serializer_traits_t::serializer_type out {ar};
    ap.serialize(out);

    serializer_traits_t::deserializer_type in {ar.data(), ar.size()};
    header h {in};
    alive_packet_t ap1 {h, in};

    CHECK_EQ(ap1.version(), header::VERSION);
    CHECK_EQ(ap1.type(), packet_enum::alive);
    CHECK_FALSE(ap1.has_checksum());
    CHECK_EQ(ap.info().id, ainfo_sample.id);
}

TEST_CASE("unreachable_packet") {
    using unreachable_packet_t = unreachable_packet<node_id>;

    unreachable_info<node_id> uinfo_sample {
          pfs::generate_uuid() // gw_id
        , pfs::generate_uuid() // sender_id
        , pfs::generate_uuid() // receiver_id
    };

    unreachable_packet_t up {uinfo_sample};

    CHECK_EQ(up.version(), header::VERSION);

    CHECK_EQ(up.type(), packet_enum::unreach);
    CHECK_FALSE(up.has_checksum());

    // Serialization/Deserialization
    archive_t ar;
    serializer_traits_t::serializer_type out {ar};
    up.serialize(out);

    serializer_traits_t::deserializer_type in {ar.data(), ar.size()};
    header h {in};
    unreachable_packet_t up1 {h, in};

    CHECK_EQ(up1.version(), header::VERSION);
    CHECK_EQ(up1.type(), packet_enum::unreach);
    CHECK_FALSE(up1.has_checksum());
    CHECK_EQ(up.info().gw_id, uinfo_sample.gw_id);
    CHECK_EQ(up.info().sender_id, uinfo_sample.sender_id);
    CHECK_EQ(up.info().receiver_id, uinfo_sample.receiver_id);
}

TEST_CASE("route_packet") {
    using route_packet_t = route_packet<node_id>;

    route_info<node_id> rinfo_sample {
          pfs::generate_uuid() // initiator_id
        , pfs::generate_uuid() // responder_id, for response only
        , std::vector<node_id> { pfs::generate_uuid(), pfs::generate_uuid() } // route
    };

    route_packet_t rp_req {packet_way_enum::request, rinfo_sample};

    CHECK_EQ(rp_req.version(), header::VERSION);
    CHECK_EQ(rp_req.type(), packet_enum::route);
    CHECK_FALSE(rp_req.has_checksum());
    CHECK_FALSE(rp_req.is_response());

    route_packet_t rp_rep {packet_way_enum::response, rinfo_sample};

    CHECK_EQ(rp_rep.version(), header::VERSION);
    CHECK_EQ(rp_rep.type(), packet_enum::route);
    CHECK_FALSE(rp_rep.has_checksum());
    CHECK(rp_rep.is_response());

    // Serialization/Deserialization
    {
        archive_t ar;
        serializer_traits_t::serializer_type out {ar};
        rp_req.serialize(out);

        serializer_traits_t::deserializer_type in {ar.data(), ar.size()};
        header h {in};
        route_packet_t rp1_req {h, in};

        CHECK_EQ(rp1_req.version(), header::VERSION);
        CHECK_EQ(rp1_req.type(), packet_enum::route);
        CHECK_FALSE(rp1_req.has_checksum());
        CHECK_FALSE(rp1_req.is_response());
        CHECK_EQ(rp1_req.info().initiator_id, rinfo_sample.initiator_id);
        REQUIRE_EQ(rp1_req.info().route.size(), 2);
        REQUIRE_EQ(rp1_req.info().route[0], rinfo_sample.route[0]);
        REQUIRE_EQ(rp1_req.info().route[1], rinfo_sample.route[1]);
    }

    {
        archive_t ar;
        serializer_traits_t::serializer_type out {ar};
        rp_rep.serialize(out);

        serializer_traits_t::deserializer_type in {ar.data(), ar.size()};
        header h {in};
        route_packet_t rp1_rep {h, in};

        CHECK_EQ(rp1_rep.version(), header::VERSION);
        CHECK_EQ(rp1_rep.type(), packet_enum::route);
        CHECK_FALSE(rp1_rep.has_checksum());
        CHECK(rp1_rep.is_response());
        CHECK_EQ(rp1_rep.info().initiator_id, rinfo_sample.initiator_id);
        CHECK_EQ(rp1_rep.info().responder_id, rinfo_sample.responder_id);
        REQUIRE_EQ(rp1_rep.info().route.size(), 2);
        REQUIRE_EQ(rp1_rep.info().route[0], rinfo_sample.route[0]);
        REQUIRE_EQ(rp1_rep.info().route[1], rinfo_sample.route[1]);
    }
}

TEST_CASE("ddata_packet") {
    using ddata_packet_t = ddata_packet;

    std::vector<char> msg_sample {'H', 'e', 'l', 'l', 'o', ',', 'W', 'o', 'r', 'l', 'd', '!',};

    bool force_checksum = true;
    ddata_packet_t ddp {force_checksum};

    CHECK_EQ(ddp.version(), header::VERSION);
    CHECK_EQ(ddp.type(), packet_enum::ddata);
    CHECK_EQ(ddp.has_checksum(), force_checksum);

    // Serialization/Deserialization
    archive_t ar;
    serializer_traits_t::serializer_type out {ar};
    ddp.serialize(out, msg_sample.data(), msg_sample.size());

    serializer_traits_t::deserializer_type in {ar.data(), ar.size()};
    header h {in};
    std::vector<char> msg;
    ddata_packet_t ddp1 {h, in, msg};

    CHECK_EQ(ddp1.version(), header::VERSION);
    CHECK_EQ(ddp1.type(), packet_enum::ddata);
    CHECK(ddp1.has_checksum());
    CHECK_EQ(msg, msg_sample);
}

TEST_CASE("gdata_packet") {
    using gdata_packet_t = gdata_packet<node_id>;

    std::vector<char> msg_sample {'H', 'e', 'l', 'l', 'o', ',', 'W', 'o', 'r', 'l', 'd', '!',};

    auto sender_id_sample = pfs::generate_uuid();
    auto received_id_sample = pfs::generate_uuid();

    bool force_checksum = true;
    gdata_packet_t gdp {sender_id_sample, received_id_sample, force_checksum};

    CHECK_EQ(gdp.version(), header::VERSION);
    CHECK_EQ(gdp.type(), packet_enum::gdata);
    CHECK_EQ(gdp.has_checksum(), force_checksum);

    // Serialization/Deserialization
    archive_t ar;
    serializer_traits_t::serializer_type out {ar};
    gdp.serialize(out, msg_sample.data(), msg_sample.size());

    serializer_traits_t::deserializer_type in {ar.data(), ar.size()};
    header h {in};
    std::vector<char> msg;
    gdata_packet_t gdp1 {h, in, msg};

    CHECK_EQ(gdp1.version(), header::VERSION);
    CHECK_EQ(gdp1.type(), packet_enum::gdata);
    CHECK(gdp1.has_checksum());
    CHECK_EQ(gdp1.sender_id(), sender_id_sample);
    CHECK_EQ(gdp1.receiver_id(), received_id_sample);
    CHECK_EQ(msg, msg_sample);
}
