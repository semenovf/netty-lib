////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.08 Initial version.
//      2021.11.17 New packet format.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "envelope.hpp"
#include "serializer.hpp"
#include "universal_id.hpp"
#include "pfs/sha256.hpp"
#include <sstream>
#include <cassert>
#include <cstring>

namespace netty {
namespace p2p {

enum class packet_type_enum: std::uint8_t {
      regular = 0x2A
    , file_credentials
    , file
};

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
          sizeof(std::underlying_type<packet_type_enum>::type) // packettype
        + sizeof(std::uint16_t)  // packetsize
        + 16                     // addresser
        + sizeof(std::uint16_t)  // payloadsize
        + sizeof(std::uint32_t)  // partcount
        + sizeof(std::uint32_t); // partindex

    static constexpr std::uint16_t MAX_PACKET_SIZE = 1430;
    static constexpr std::uint16_t MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE - PACKET_HEADER_SIZE;

    packet_type_enum packettype;
    std::uint16_t   packetsize;  // Packet size
    universal_id    addresser;   // Addresser (sender) UUID
    std::uint16_t   payloadsize;
    std::uint32_t   partcount;   // Total count of parts
    std::uint32_t   partindex;   // Part index (starts from 1)
    char            payload[MAX_PAYLOAD_SIZE];
};

struct file_credentials
{
    std::string filename;
    std::uint64_t filesize;
    pfs::crypto::sha256_digest sha256;
    //char sha256[32];        // SHA256 checksum for file
};

// template <typename Consumer>
// void split_into_packets (std::uint16_t packet_size
//     , universal_id addresser
//     , packet_type_enum packettype
//     , char const * data, std::streamsize len
//     , Consumer && consumer)
// {
//     assert(packet_size <= packet::MAX_PACKET_SIZE
//         && packet_size > packet::PACKET_HEADER_SIZE);
//
//     assert(packettype == packet_type_enum::regular
//         || packettype == packet_type_enum::file_credentials);
//
//     auto payload_size = packet_size - packet::PACKET_HEADER_SIZE;
//     auto remain_len   = len;
//     char const * remain_data = data;
//     std::uint32_t partindex  = 1;
//     std::uint32_t partcount  = len / payload_size
//         + (len % payload_size ? 1 : 0);
//
//     while (remain_len) {
//         packet p;
//
//         p.packettype  = packettype;
//         p.packetsize  = packet::PACKET_HEADER_SIZE + payload_size;
//         p.addresser   = addresser;
//         p.payloadsize = remain_len > payload_size
//             ? payload_size
//             : static_cast<std::uint16_t>(remain_len);
//         p.partcount   = partcount;
//         p.partindex   = partindex++;
//         std::memset(p.payload, 0, payload_size);
//         std::memcpy(p.payload, remain_data, p.payloadsize);
//
//         remain_len -= p.payloadsize;
//         remain_data += p.payloadsize;
//
//         consumer(std::move(p));
//     }
// }
//
// template <typename Consumer>
// inline void split_into_packets (std::uint16_t packet_size
//     , universal_id addresser
//     , char const * data, std::streamsize len
//     , Consumer && consumer)
// {
//     split_into_packets<Consumer>(packet_size, addresser
//         , packet_type_enum::regular, data, len
//         , std::forward<Consumer>(consumer));
// }

// struct file_packet
// {
//     static constexpr std::uint8_t PACKET_HEADER_SIZE =
//           sizeof(std::underlying_type<packet_type_enum>::type) // packettype
//         + sizeof(std::uint16_t)  // packetsize
//         + 16                     // addresser (sender)
//         + 16                     // fileid
//         + sizeof(std::uint64_t)  // fileoffset
//         + sizeof(std::uint16_t); // payloadsize
//
//     static constexpr std::size_t MAX_PACKET_SIZE = 1430;
//     static constexpr std::size_t MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE - PACKET_HEADER_SIZE;
//
//     packet_type_enum packettype;
//     std::uint16_t   packetsize;  // Packet size
//     universal_id    addresser;   // Addresser (sender) UUID
//     universal_id    fileid;      // Unique identifier for file
//     std::uint64_t   fileoffset;
//     std::uint16_t   payloadsize;
//     char            payload[MAX_PAYLOAD_SIZE];
// };

// inline void save (cereal::BinaryOutputArchive & ar, packet_type_enum const & pt)
// {
//     auto x = static_cast<std::underlying_type<packet_type_enum>::type>(pt);
//     ar << pfs::to_network_order(x);
// }

// inline void load (cereal::BinaryInputArchive & ar, packet_type_enum & pt)
// {
//     std::underlying_type<packet_type_enum>::type x;
//     ar >> ntoh_wrapper<decltype(x)>(x);
//
//     switch (x) {
//         case static_cast<int>(packet_type_enum::regular):
//             pt = packet_type_enum::regular;
//             break;
//         case static_cast<int>(packet_type_enum::file_credentials):
//             pt = packet_type_enum::file_credentials;
//             break;
//         case static_cast<int>(packet_type_enum::file):
//             pt = packet_type_enum::file;
//             break;
//         default:
//             pt = packet_type_enum::invalid;
//             break;
//     }
// }

inline void save (cereal::BinaryOutputArchive & ar, packet const & pkt)
{
    ar << pkt.packettype
        << pfs::to_network_order(pkt.packetsize)
        << pkt.addresser
        << pfs::to_network_order(pkt.payloadsize)
        << pfs::to_network_order(pkt.partcount)
        << pfs::to_network_order(pkt.partindex)
        << cereal::binary_data(pkt.payload, sizeof(pkt.payload));
}

inline void load (cereal::BinaryInputArchive & ar, packet & pkt)
{
    ar >> pkt.packettype
        >> ntoh_wrapper<decltype(pkt.packetsize)>{pkt.packetsize}
        >> pkt.addresser
        >> ntoh_wrapper<decltype(pkt.payloadsize)>(pkt.payloadsize)
        >> ntoh_wrapper<decltype(pkt.partcount)>(pkt.partcount)
        >> ntoh_wrapper<decltype(pkt.partindex)>(pkt.partindex)
        >> cereal::binary_data(pkt.payload, sizeof(pkt.payload));
}

inline void save (cereal::BinaryOutputArchive & ar, file_credentials const & fc)
{
    ar << fc.filename
        << pfs::to_network_order(fc.filesize)
        << cereal::binary_data(fc.sha256.value.data(), fc.sha256.value.size());
}

inline void load (cereal::BinaryInputArchive & ar, file_credentials & fc)
{
    ar >> fc.filename
        >> ntoh_wrapper<decltype(fc.filesize)>{fc.filesize}
        >> cereal::binary_data(fc.sha256.value.data(), fc.sha256.value.size());
}

}} // namespace netty::p2p
