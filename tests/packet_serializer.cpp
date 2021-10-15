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
#include "pfs/net/p2p/packet.hpp"
#include "pfs/net/p2p/envelope.hpp"

namespace p2p = pfs::net::p2p;

using uuid_t            = pfs::uuid_t;
using packet_t          = p2p::packet<128>;
using output_envelope_t = p2p::output_envelope<>;

TEST_CASE("packet_serialization")
{
    packet_t pkt;
    auto sender_uuid = pfs::from_string<uuid_t>("01FH7H6YJB8XK9XNNZYR0WYDJ1");
    packet_t::seqnum_type initial_sn = 42;
    std::string payload {"Hello, World!"};

    auto next_sn = p2p::split_into_packets<128>(sender_uuid, initial_sn
        , payload.data(), payload.size(), [& pkt] (packet_t && p) {
        pkt = std::move(p);
    });

    output_envelope_t oe;
    oe.seal(pkt);

    CHECK_EQ(next_sn, 43);
    CHECK_EQ(oe.data().size(), 128);
}
