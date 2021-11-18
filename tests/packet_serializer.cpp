////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// License: see LICENSE file
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.12 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "pfs/uuid.hpp"
#include "pfs/net/p2p/envelope.hpp"
#include "pfs/net/p2p/packet.hpp"

namespace p2p = pfs::net::p2p;

namespace {
    constexpr std::size_t PACKET_SIZE = 64;
}

using uuid_t            = pfs::uuid_t;
using packet_t          = p2p::packet;
using output_envelope_t = p2p::output_envelope<>;
using input_envelope_t  = p2p::input_envelope<>;

TEST_CASE("packet_serialization")
{
    packet_t pkt;
    auto sender_uuid = "01FH7H6YJB8XK9XNNZYR0WYDJ1"_uuid;
    std::string payload {"Hello, World!"};

    p2p::split_into_packets(PACKET_SIZE, sender_uuid
        , payload.data(), payload.size(), [& pkt] (packet_t && p) {
            pkt = std::move(p);
        });

    output_envelope_t oe;
    oe.seal(pkt);

    CHECK((oe.data().size() == packet_t::MAX_PACKET_SIZE));

    input_envelope_t ie {oe.data()};

    ie.unseal(pkt);

    CHECK(pkt.packetsize == PACKET_SIZE);
    CHECK_EQ(pkt.uuid, sender_uuid);
    CHECK_EQ(pkt.partcount, 1);
    CHECK_EQ(pkt.partindex, 1);
    CHECK_EQ(pkt.payloadsize, 13);
    CHECK_EQ(payload, std::string(pkt.payload, pkt.payloadsize));
}
