////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.09.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "universal_id.hpp"
#include "pfs/crc16.hpp"
#include "pfs/universal_id_crc.hpp"

namespace netty {
namespace p2p {

struct hello_packet
{
    static constexpr std::size_t PACKET_SIZE
        = 4 * sizeof(char)
        + 16
        + 2 * sizeof(std::uint16_t)
        + sizeof(std::uint32_t)
        + sizeof(std::int64_t)
        + sizeof(std::int16_t);

    char greeting[4] = {'H', 'E', 'L', 'O'};
    universal_id uuid;
    std::uint16_t port {0};  // Port that will accept connections
    std::uint16_t expiration_interval {0}; // Expiration interval in seconds
    std::uint32_t counter {0};
    std::int64_t  timestamp; // UTC timestamp in milliseconds since epoch
    std::int16_t  crc16;
};

inline std::int16_t crc16_of (hello_packet const & pkt)
{
    auto crc16 = pfs::crc16_of_ptr(pkt.greeting, sizeof(pkt.greeting), 0);
    crc16 = pfs::crc16_all_of(crc16, pkt.uuid, pkt.port, pkt.expiration_interval
        , pkt.counter, pkt.timestamp);
    return crc16;
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
