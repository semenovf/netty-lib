////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.08 Initial version.
//      2021.11.17 New packet format.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "envelope.hpp"
#include "file.hpp"
#include "serializer.hpp"
#include "universal_id.hpp"
#include "pfs/sha256.hpp"
#include <cereal/types/vector.hpp>
#include <future>
#include <sstream>
#include <cassert>
#include <cstring>

namespace netty {
namespace p2p {

enum class packet_type: std::uint8_t {
      regular = 0x2A
    , hello
    , file_credentials
    , file_request
    , file_stop    // Stop/pause file transferring
    , file_begin   // Start downloading
    , file_chunk
    , file_end
    , file_state
};

enum class file_status: std::uint8_t {
      success = 0x2A // File received successfully
//    , end            // File transfer complete
//    , checksum       // Checksum error
};

// Packet structure
//------------------------------------------------------------------------------
// [T][SS][uuuuuuuuuuuuuuuu][ss][PPPP][pppp][--PAYLOAD--]
//  ^  ^          ^          ^     ^    ^
//  |  |          |          |     |    |________ Part index (4 bytes)
//  |  |          |          |     |_____________ Total count of parts (4 bytes)
//  |  |          |          |___________________ Payload size (2 bytes)
//  |  |          |______________________________ Addresser (16 bytes)
//  |  |_________________________________________ Packet size (2 bytes)
//  |____________________________________________ Packet type (1 byte)
//
struct packet
{
    static constexpr std::uint8_t PACKET_HEADER_SIZE =
          sizeof(std::underlying_type<packet_type>::type) // packettype
        + sizeof(std::uint16_t)  // packetsize
        + 16                     // addresser
        + sizeof(std::uint16_t)  // payloadsize
        + sizeof(std::uint32_t)  // partcount
        + sizeof(std::uint32_t); // partindex

    static constexpr std::uint16_t MAX_PACKET_SIZE = 1430;
    static constexpr std::uint16_t MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE - PACKET_HEADER_SIZE;

    packet_type packettype;
    std::uint16_t packetsize;  // Packet size
    universal_id  addresser;   // Addresser (sender) UUID
    std::uint16_t payloadsize;
    std::uint32_t partcount;   // Total count of parts
    std::uint32_t partindex;   // Part index (starts from 1)
    char          payload[MAX_PAYLOAD_SIZE];
};

// `addresser` field of packet is a payload for this packet type
struct hello {};

struct file_credentials
{
    universal_id fileid;
    std::string  filename;
    std::int32_t filesize;
    std::int32_t offset;
};

struct file_request
{
    universal_id fileid;
    std::int32_t offset;
};

struct file_stop
{
    universal_id fileid;
};

struct file_begin
{
    universal_id fileid;
    std::int32_t offset;
};

struct file_chunk
{
    universal_id fileid;
    std::int32_t offset;
    std::int32_t chunksize;
    std::vector<char> chunk;
};

struct file_end
{
    universal_id fileid;
    //pfs::crypto::sha256_digest checksum;
};

struct file_state
{
    universal_id fileid;
    file_status status;
};

////////////////////////////////////////////////////////////////////////////////
// packet
////////////////////////////////////////////////////////////////////////////////
inline void save (cereal::BinaryOutputArchive & ar, packet const & pkt)
{
    ar << pkt.packettype
        << pfs::to_network_order(pkt.packetsize)
        << pkt.addresser
        << pfs::to_network_order(pkt.payloadsize)
        << pfs::to_network_order(pkt.partcount)
        << pfs::to_network_order(pkt.partindex)
        << cereal::binary_data(pkt.payload, pkt.packetsize - packet::PACKET_HEADER_SIZE);
}

inline void load (cereal::BinaryInputArchive & ar, packet & pkt)
{
    ar >> pkt.packettype;
    ar >> ntoh_wrapper<decltype(pkt.packetsize)>{pkt.packetsize};
    ar >> pkt.addresser;
    ar >> ntoh_wrapper<decltype(pkt.payloadsize)>(pkt.payloadsize);
    ar >> ntoh_wrapper<decltype(pkt.partcount)>(pkt.partcount);
    ar >> ntoh_wrapper<decltype(pkt.partindex)>(pkt.partindex);
    ar >> cereal::binary_data(pkt.payload, pkt.packetsize - packet::PACKET_HEADER_SIZE);
}

////////////////////////////////////////////////////////////////////////////////
// hello
////////////////////////////////////////////////////////////////////////////////
inline void save (cereal::BinaryOutputArchive & ar, hello const & h)
{}

inline void load (cereal::BinaryInputArchive & ar, hello & h)
{}

////////////////////////////////////////////////////////////////////////////////
// file_credentials
////////////////////////////////////////////////////////////////////////////////
inline void save (cereal::BinaryOutputArchive & ar, file_credentials const & fc)
{
    ar << fc.fileid
        << fc.filename
        << pfs::to_network_order(fc.filesize)
        << pfs::to_network_order(fc.offset);
}

inline void load (cereal::BinaryInputArchive & ar, file_credentials & fc)
{
    ar >> fc.fileid
        >> fc.filename
        >> ntoh_wrapper<decltype(fc.filesize)>{fc.filesize}
        >> ntoh_wrapper<decltype(fc.offset)>{fc.offset};
}

////////////////////////////////////////////////////////////////////////////////
// file_request
////////////////////////////////////////////////////////////////////////////////
inline void save (cereal::BinaryOutputArchive & ar, file_request const & fr)
{
    ar << fr.fileid << pfs::to_network_order(fr.offset);
}

inline void load (cereal::BinaryInputArchive & ar, file_request & fr)
{
    ar >> fr.fileid >> ntoh_wrapper<decltype(fr.offset)>{fr.offset};
}

////////////////////////////////////////////////////////////////////////////////
// file_stop
////////////////////////////////////////////////////////////////////////////////
inline void save (cereal::BinaryOutputArchive & ar, file_stop const & fs)
{
    ar << fs.fileid;
}

inline void load (cereal::BinaryInputArchive & ar, file_stop & fs)
{
    ar >> fs.fileid;
}

////////////////////////////////////////////////////////////////////////////////
// file_chunk
////////////////////////////////////////////////////////////////////////////////
inline void save (cereal::BinaryOutputArchive & ar, file_chunk const & fc)
{
    ar << fc.fileid
        << pfs::to_network_order(fc.offset)
        << pfs::to_network_order(fc.chunksize)
        << fc.chunk;
}

inline void load (cereal::BinaryInputArchive & ar, file_chunk & fc)
{
    ar >> fc.fileid
        >> ntoh_wrapper<decltype(fc.offset)>{fc.offset}
        >> ntoh_wrapper<decltype(fc.chunksize)>{fc.chunksize}
        >> fc.chunk;
}

////////////////////////////////////////////////////////////////////////////////
// file_begin
////////////////////////////////////////////////////////////////////////////////
inline void save (cereal::BinaryOutputArchive & ar, file_begin const & fb)
{
    ar << fb.fileid << pfs::to_network_order(fb.offset);
}

inline void load (cereal::BinaryInputArchive & ar, file_begin & fb)
{
    ar >> fb.fileid >> ntoh_wrapper<decltype(fb.offset)>{fb.offset};
}

////////////////////////////////////////////////////////////////////////////////
// file_end
////////////////////////////////////////////////////////////////////////////////
inline void save (cereal::BinaryOutputArchive & ar, file_end const & fe)
{
    ar << fe.fileid;
//         << cereal::binary_data(fe.checksum.data(), fe.checksum.size());
}

inline void load (cereal::BinaryInputArchive & ar, file_end & fe)
{
    ar >> fe.fileid;
//         >> cereal::binary_data(fe.checksum.data(), fe.checksum.size());
}

////////////////////////////////////////////////////////////////////////////////
// file_state
////////////////////////////////////////////////////////////////////////////////
inline void save (cereal::BinaryOutputArchive & ar, file_state const & fs)
{
    ar << fs.fileid << fs.status;
}

inline void load (cereal::BinaryInputArchive & ar, file_state & fs)
{
    ar >> fs.fileid >> fs.status;
}

}} // namespace netty::p2p
