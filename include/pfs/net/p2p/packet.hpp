////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/crc32.hpp"
#include "pfs/uuid.hpp"
#include "pfs/crc32.hpp"
#include <cereal/archives/portable_binary.hpp>
#include <sstream>
#include <vector>

namespace pfs {
namespace net {
namespace p2p {

// [BE][uuuuuuuuuuuuuuuu][nnnnnnnn][PPPPPPPP][pppppppp][--PAYLOAD--][cccccccc][ED]
//  ^          ^              ^         ^        ^                      ^      ^
//  |          |              |         |        |                      |      |
//  |          |              |         |        |                      |      |__ End flag (1 byte)
//  |          |              |         |        |                      |_________ CRC32 (4 bytes)
//  |          |              |         |        |________________________________ Part index (4 bytes)
//  |          |              |         |_________________________________________ Total count of parts (4 bytes)
//  |          |              |___________________________________________________ Sequence number (4 bytes)
//  |          |__________________________________________________________________ UUID (16 bytes)
//  |_____________________________________________________________________________ Start flag (1 byte)

template <std::size_t PacketSize>
struct packet
{
    using seqnum_type = std::uint32_t;

    static constexpr std::uint8_t START_FLAG   = 0xBE;
    static constexpr std::uint8_t END_FLAG     = 0xED;
    static constexpr std::size_t  PACKET_SIZE  = PacketSize;
    static constexpr std::size_t  PAYLOAD_SIZE = PACKET_SIZE
        -  1  // startflag
        - 16  // uuid
        -  4  // sn
        -  4  // partcount
        -  4  // partindex
        -  2  // payloadsize
        -  4  // crc32
        -  1; // endflag

    std::uint8_t  startflag {START_FLAG};
    uuid_t        uuid;                  // Sender UUID
    seqnum_type   sn;
    std::uint32_t partcount;             // Total count of parts
    std::uint32_t partindex;             // Part index (starts from 1)
    std::uint16_t payloadsize;
    char          payload[PAYLOAD_SIZE];
    std::int32_t  crc32;                 // CRC32 of packet data excluding
                                         // startflag and endflag fields
    std::uint8_t  endflag {END_FLAG};
};

template <std::size_t PacketSize>
constexpr std::size_t packet<PacketSize>::PACKET_SIZE;

template <std::size_t PacketSize>
constexpr std::size_t packet<PacketSize>::PAYLOAD_SIZE;

template <std::size_t PacketSize>
inline std::int32_t crc32_of (packet<PacketSize> const & pkt)
{
    using packet_type = packet<PacketSize>;

    auto crc32 = pfs::crc32_all_of(0, pkt.uuid, pkt.sn, pkt.partcount
        , pkt.partindex, pkt.payloadsize);

    return pfs::crc32_of_ptr(pkt.payload, packet_type::PAYLOAD_SIZE, crc32);
}

template <std::size_t PacketSize, typename Recipient>
typename packet<PacketSize>::seqnum_type split_into_packets (uuid_t sender_uuid
    , typename packet<PacketSize>::seqnum_type initial_sn
    , char const * data, std::size_t len
    , Recipient && recipient)
{
    using packet_type = packet<PacketSize>;
    using seqnum_type = typename packet_type::seqnum_type;

    std::size_t remain_len = len;
    char const * remain_data = data;
    seqnum_type result = initial_sn;
    std::uint32_t partindex = 1;
    std::uint32_t partcount = len / packet_type::PAYLOAD_SIZE
        + (len % packet_type::PAYLOAD_SIZE ? 1 : 0);

    while (remain_len) {
        packet_type p;

        p.uuid        = sender_uuid;
        p.sn          = result++;
        p.partcount   = partcount;
        p.partindex   = partindex++;
        p.payloadsize = remain_len > packet_type::PAYLOAD_SIZE
            ? packet_type::PAYLOAD_SIZE
            : remain_len;
        std::memset(p.payload, 0, packet_type::PAYLOAD_SIZE);
        std::memcpy(p.payload, remain_data, p.payloadsize);

        p.crc32 = crc32_of(p);

        remain_len -= p.payloadsize;

        recipient(std::move(p));
    }

    return result;
}

template <std::size_t PacketSize, typename Archiver>
void serialize (Archiver & ar, packet<PacketSize> const & pkt)
{
    ar << pkt.startflag
        << pkt.uuid
        << pkt.sn
        << pkt.partcount
        << pkt.partindex
        << pkt.payloadsize
        << cereal::binary_data(pkt.payload, sizeof(pkt.payload))
        << pkt.crc32
        << pkt.endflag
        ;
}

}}} // namespace pfs::net::p2p

namespace cereal
{
    using pfs::net::p2p::packet;

    template <std::size_t PacketSize>
    void save (BinaryOutputArchive & ar, packet<PacketSize> const & pkt)
    {
        ar << pkt.startflag
            << pkt.uuid
            << pkt.sn
            << pkt.partcount
            << pkt.partindex
            << pkt.payloadsize
            << cereal::binary_data(pkt.payload, sizeof(pkt.payload))
            << pkt.crc32
            << pkt.endflag;
    }

    template <std::size_t PacketSize>
    void load (BinaryInputArchive & ar, packet<PacketSize> & pkt)
    {
        using packet_type = packet<PacketSize>;

        ar >> pkt.startflag
            >> pkt.uuid
            >> pkt.sn
            >> pkt.partcount
            >> pkt.partindex
            >> pkt.payloadsize
            >> cereal::binary_data(pkt.payload, sizeof(pkt.payload))
            >> pkt.crc32 >> pkt.endflag;

//     if (pkt.startflag != packet_type::START_FLAG)
//         return std::make_pair("bad packet: bad START flag", packet_type{});
//
//     if (pkt.startflag != packet_type::END_FLAG)
//         return std::make_pair("bad packet: bad END flag", packet_type{});
//
//     std::int32_t crc32 = pfs::crc32_all_of(0, pkt.uuid
//         , pkt.sn, pkt.partcount, pkt.partindex);
//
//     crc32 = pfs::crc32_of_ptr(pkt.payload, packet_type::PAYLOAD_SIZE, crc32);
//
//     if (crc32 != pkt.crc32)
//         return std::make_pair("bad CRC32", packet_type{});
//
//     return std::make_pair(std::string{}, std::move(pkt));
    }
} // namespace cereal
