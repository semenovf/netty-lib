////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "serializer.hpp"
#include "pfs/crc32.hpp"
#include "pfs/uuid.hpp"
#include "pfs/uuid_crc.hpp"
#include <sstream>

namespace pfs {
namespace net {
namespace p2p {

// [BE][uuuuuuuuuuuuuuuu][PPPPPPPP][pppppppp][--PAYLOAD--][cccccccc][ED]
//  ^          ^              ^        ^                      ^      ^
//  |          |              |        |                      |      |
//  |          |              |        |                      |      |__ End flag (1 byte)
//  |          |              |        |                      |_________ CRC32 (4 bytes)
//  |          |              |        |________________________________ Part index (4 bytes)
//  |          |              |_________________________________________ Total count of parts (4 bytes)
//  |          |________________________________________________________ UUID (16 bytes)
//  |___________________________________________________________________ Start flag (1 byte)

inline constexpr std::size_t CALCULATE_PACKET_BASE_SIZE ()
{
    return sizeof(std::uint8_t) // startflag
        + 16                    // uuid
        + sizeof(std::uint32_t) // partcount
        + sizeof(std::uint32_t) // partindex
        + sizeof(std::uint16_t) // payloadsize
        + sizeof(std::uint8_t); // endflag
}

inline constexpr std::size_t CALCULATE_PACKET_SIZE (std::size_t payload_size)
{
    return payload_size + CALCULATE_PACKET_BASE_SIZE();
}

template <std::size_t PacketSize>
struct packet
{
    static constexpr std::uint8_t START_FLAG   = 0xBE;
    static constexpr std::uint8_t END_FLAG     = 0xED;
    static constexpr std::size_t  PACKET_SIZE  = PacketSize;
    static constexpr std::size_t  PAYLOAD_SIZE = PACKET_SIZE - CALCULATE_PACKET_BASE_SIZE();

    std::uint8_t    startflag {START_FLAG};
    uuid_t          uuid;                  // Sender UUID
    std::uint32_t   partcount;             // Total count of parts
    std::uint32_t   partindex;             // Part index (starts from 1)
    std::uint16_t   payloadsize;
    char            payload[PAYLOAD_SIZE];
    std::uint8_t    endflag {END_FLAG};

//     static_assert(PAYLOAD_SIZE >= sizeof(?)
//         , "PACKET_SIZE must be increased to meet minimum payload requirements (32)");
};

template <std::size_t PacketSize>
constexpr std::uint8_t packet<PacketSize>::START_FLAG;

template <std::size_t PacketSize>
constexpr std::uint8_t packet<PacketSize>::END_FLAG;

template <std::size_t PacketSize>
constexpr std::size_t packet<PacketSize>::PACKET_SIZE;

template <std::size_t PacketSize>
constexpr std::size_t packet<PacketSize>::PAYLOAD_SIZE;

template <std::size_t PacketSize, typename Consumer>
void split_into_packets (uuid_t sender_uuid
    , char const * data, std::size_t len
    , Consumer && consumer)
{
    using packet_type = packet<PacketSize>;

    std::size_t remain_len = len;
    char const * remain_data = data;
    std::uint32_t partindex = 1;
    std::uint32_t partcount = len / packet_type::PAYLOAD_SIZE
        + (len % packet_type::PAYLOAD_SIZE ? 1 : 0);

    while (remain_len) {
        packet_type p;

        p.uuid        = sender_uuid;
        p.partcount   = partcount;
        p.partindex   = partindex++;
        p.payloadsize = remain_len > packet_type::PAYLOAD_SIZE
            ? packet_type::PAYLOAD_SIZE
            : remain_len;
        std::memset(p.payload, 0, packet_type::PAYLOAD_SIZE);
        std::memcpy(p.payload, remain_data, p.payloadsize);

        // This will be calculated later (in save() function)
        // p.crc32 = crc32_of(p);

        remain_len -= p.payloadsize;

        consumer(std::move(p));
    }
}

template <std::size_t PacketSize>
void save (cereal::BinaryOutputArchive & ar, packet<PacketSize> const & pkt)
{
    ar << pfs::to_network_order(pkt.startflag)
        << pfs::to_network_order(pkt.uuid)
        << pfs::to_network_order(pkt.partcount)
        << pfs::to_network_order(pkt.partindex)
        << pfs::to_network_order(pkt.payloadsize)
        << cereal::binary_data(pkt.payload, sizeof(pkt.payload))
        << pfs::to_network_order(pkt.endflag);
}

template <std::size_t PacketSize>
void load (cereal::BinaryInputArchive & ar, packet<PacketSize> & pkt)
{
    ar >> ntoh_wrapper<decltype(pkt.startflag)>{pkt.startflag}
        >> ntoh_wrapper<decltype(pkt.uuid)>(pkt.uuid)
        >> ntoh_wrapper<decltype(pkt.partcount)>(pkt.partcount)
        >> ntoh_wrapper<decltype(pkt.partindex)>(pkt.partindex)
        >> ntoh_wrapper<decltype(pkt.payloadsize)>(pkt.payloadsize)
        >> cereal::binary_data(pkt.payload, sizeof(pkt.payload))
        >> ntoh_wrapper<decltype(pkt.endflag)>(pkt.endflag);
}

template <std::size_t PacketSize>
inline bool validate (packet<PacketSize> const & pkt)
{
    if (pkt.startflag != packet<PacketSize>::START_FLAG)
        return false;

    if (pkt.endflag != packet<PacketSize>::END_FLAG)
        return false;

    return true;
}

}}} // namespace pfs::net::p2p
