////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.08 Initial version.
//      2021.11.17 New packet format.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "serializer.hpp"
#include "uuid.hpp"
#include "pfs/crc32.hpp"
#include <sstream>
#include <cassert>
#include <cstring>

namespace pfs {
namespace net {
namespace p2p {

// [SSSS][uuuuuuuuuuuuuuuu][PPPPPPPP][pppppppp][ssss][--PAYLOAD--]
//    ^          ^              ^        ^        ^
//    |          |              |        |        |__ Payload size (2 bytes)
//    |          |              |        |___________ Part index (4 bytes)
//    |          |              |____________________ Total count of parts (4 bytes)
//    |          |___________________________________ UUID (16 bytes)
//    |______________________________________________ Packet size (2 bytes)

struct packet
{
    static constexpr std::uint8_t PACKET_HEADER_SIZE =
          sizeof(std::uint16_t)  // packetsize
        + 16                     // uuid
        + sizeof(std::uint32_t)  // partcount
        + sizeof(std::uint32_t)  // partindex
        + sizeof(std::uint16_t); // payloadsize
    static constexpr std::size_t MAX_PACKET_SIZE = 1430;
    static constexpr std::size_t MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE - PACKET_HEADER_SIZE;

    std::uint16_t   packetsize;  // Total packet size
    uuid_t          uuid;        // Sender UUID
    std::uint32_t   partcount;   // Total count of parts
    std::uint32_t   partindex;   // Part index (starts from 1)
    std::uint16_t   payloadsize;
    char            payload[MAX_PAYLOAD_SIZE];
};

template <typename Consumer>
void split_into_packets (std::size_t packet_size
    , uuid_t sender_uuid
    , char const * data, std::streamsize len
    , Consumer && consumer)
{
    assert(packet_size <= packet::MAX_PACKET_SIZE
        && packet_size > packet::PACKET_HEADER_SIZE);

    std::size_t payload_size = packet_size - packet::PACKET_HEADER_SIZE;
    std::size_t remain_len   = len;
    char const * remain_data = data;
    std::uint32_t partindex  = 1;
    std::uint32_t partcount  = len / payload_size
        + (len % payload_size ? 1 : 0);

    while (remain_len) {
        packet p;

        p.packetsize  = packet::PACKET_HEADER_SIZE + payload_size;
        p.uuid        = sender_uuid;
        p.partcount   = partcount;
        p.partindex   = partindex++;
        p.payloadsize = remain_len > payload_size
            ? payload_size
            : remain_len;
        std::memset(p.payload, 0, payload_size);
        std::memcpy(p.payload, remain_data, p.payloadsize);

        remain_len -= p.payloadsize;
        remain_data += p.payloadsize;

        consumer(std::move(p));
    }
}

inline void save (cereal::BinaryOutputArchive & ar, packet const & pkt)
{
    ar << pfs::to_network_order(pkt.packetsize)
        << pkt.uuid
        << pfs::to_network_order(pkt.partcount)
        << pfs::to_network_order(pkt.partindex)
        << pfs::to_network_order(pkt.payloadsize)
        << cereal::binary_data(pkt.payload, sizeof(pkt.payload));
}

inline void load (cereal::BinaryInputArchive & ar, packet & pkt)
{
    ar >> ntoh_wrapper<decltype(pkt.packetsize)>{pkt.packetsize}
        >> pkt.uuid
        >> ntoh_wrapper<decltype(pkt.partcount)>(pkt.partcount)
        >> ntoh_wrapper<decltype(pkt.partindex)>(pkt.partindex)
        >> ntoh_wrapper<decltype(pkt.payloadsize)>(pkt.payloadsize)
        >> cereal::binary_data(pkt.payload, sizeof(pkt.payload));
}

}}} // namespace pfs::net::p2p
