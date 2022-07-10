////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.09.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "serializer.hpp"
#include "universal_id.hpp"
#include "pfs/crc16.hpp"
#include "pfs/universal_id_crc.hpp"
#include <cereal/archives/binary.hpp>

namespace netty {
namespace p2p {

struct hello_packet
{
    static constexpr std::size_t PACKET_SIZE = 4 + 16 + 2 + 2;

    char greeting[4] = {'H', 'E', 'L', 'O'};
    universal_id uuid;
    std::uint16_t port {0};
    std::int16_t crc16;
};

inline std::int16_t crc16_of (hello_packet const & pkt)
{
    auto crc16 = pfs::crc16_of_ptr(pkt.greeting, sizeof(pkt.greeting), 0);
    crc16 = pfs::crc16_all_of(crc16, pkt.uuid, pkt.port);
    return crc16;
}

inline void save (cereal::BinaryOutputArchive & ar, hello_packet const & pkt)
{
    ar  << pkt.greeting[0]
        << pkt.greeting[1]
        << pkt.greeting[2]
        << pkt.greeting[3]
        << pkt.uuid
        << pfs::to_network_order(pkt.port)
        << pfs::to_network_order(crc16_of(pkt));
}

inline void load (cereal::BinaryInputArchive & ar, hello_packet & pkt)
{
    ar  >> pkt.greeting[0]
        >> pkt.greeting[1]
        >> pkt.greeting[2]
        >> pkt.greeting[3]
        >> pkt.uuid
        >> ntoh_wrapper<decltype(pkt.port)>(pkt.port)
        >> ntoh_wrapper<decltype(pkt.crc16)>(pkt.crc16);
}

inline bool is_valid (hello_packet const & pkt)
{
    if (!(pkt.greeting[0] == 'H'
            && pkt.greeting[1] == 'E'
            && pkt.greeting[2] == 'L'
            && pkt.greeting[3] == 'O')) {

        return false;
    }

    if (crc16_of(pkt) != pkt.crc16)
        return false;

    return true;
}

}} // namespace netty::p2p
