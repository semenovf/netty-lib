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
    , file_credentials
    , file_request
    , file_chunk
    , file_state
};

enum class file_status: std::uint8_t {
      success = 0x2A // File received successfully
    , interrupt      // File transfer error, need interrupt
    , end            // File transfer complete
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
// File transfer algorithm
//------------------------------------------------------------------------------
// Send file (initial)
//
// Sender                             Receiver
//   |                                   |
//   |-------file credentials ---------->|
//   |                                   |
//   |<------file request----------------|
//   |                                   |
//   |-------file chunk----------------->|
//   |-------file chunk----------------->|
//   |               ...                 |
//   |-------file chunk----------------->|
//   |-------file state(end)------------>|
//   |<------file state(success)---------|
//
// By request, initiating file transfer
//
// Sender                             Receiver
//   |                                   |
//   |<------file request----------------|
//   |                                   |
//   |-------file chunk----------------->|
//   |-------file chunk----------------->|
//   |               ...                 |
//   |-------file chunk----------------->|
//   |-------file state(end)------------>|
//   |<------file state(success)---------|
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
    std::uint16_t   packetsize;  // Packet size
    universal_id    addresser;   // Addresser (sender) UUID
    std::uint16_t   payloadsize;
    std::uint32_t   partcount;   // Total count of parts
    std::uint32_t   partindex;   // Part index (starts from 1)
    char            payload[MAX_PAYLOAD_SIZE];
};

struct file_credentials
{
    fileid_t     fileid;
    std::string  filename;
    std::int32_t filesize;
    std::int32_t offset;
    pfs::crypto::sha256_digest sha256;
};

struct file_request
{
    fileid_t     fileid;
    std::int32_t offset;
};

struct file_chunk
{
    fileid_t     fileid;
    std::int32_t offset;
    std::int32_t chunksize;
    std::vector<char> chunk;
};

struct file_state
{
    fileid_t fileid;
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
    ar >> pkt.packettype
        >> ntoh_wrapper<decltype(pkt.packetsize)>{pkt.packetsize}
        >> pkt.addresser
        >> ntoh_wrapper<decltype(pkt.payloadsize)>(pkt.payloadsize)
        >> ntoh_wrapper<decltype(pkt.partcount)>(pkt.partcount)
        >> ntoh_wrapper<decltype(pkt.partindex)>(pkt.partindex)
        >> cereal::binary_data(pkt.payload, pkt.packetsize - packet::PACKET_HEADER_SIZE);
}

////////////////////////////////////////////////////////////////////////////////
// file_credentials
////////////////////////////////////////////////////////////////////////////////
inline void save (cereal::BinaryOutputArchive & ar, file_credentials const & fc)
{
    ar << fc.fileid
        << fc.filename
        << pfs::to_network_order(fc.filesize)
        << pfs::to_network_order(fc.offset)
        << cereal::binary_data(fc.sha256.data(), fc.sha256.size());
}

inline void load (cereal::BinaryInputArchive & ar, file_credentials & fc)
{
    ar >> fc.fileid
        >> fc.filename
        >> ntoh_wrapper<decltype(fc.filesize)>{fc.filesize}
        >> ntoh_wrapper<decltype(fc.offset)>{fc.offset}
        >> cereal::binary_data(fc.sha256.data(), fc.sha256.size());
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
