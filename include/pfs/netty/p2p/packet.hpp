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
#include "serializer.hpp"
#include "universal_id.hpp"
#include "pfs/sha256.hpp"
#include <future>
#include <sstream>
#include <cassert>
#include <cstring>

#if NETTY_P2P__CEREAL_ENABLED
#   include <cereal/types/vector.hpp>
#else
#   error "No default serialization implementation"
#endif

namespace netty {
namespace p2p {

using chunksize_t = std::int32_t;
using message_id_t = std::uint64_t;

class default_message_id_generator
{
    message_id_t _value {0};

public:
    message_id_t operator () () noexcept
    {
        return ++_value;
    }
};

enum class packet_type_enum: std::uint8_t {
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
          sizeof(std::underlying_type<packet_type_enum>::type) // packettype
        + sizeof(std::uint16_t)  // packetsize
        + 16                     // addresser
        + sizeof(std::uint16_t)  // payloadsize
        + sizeof(std::uint32_t)  // partcount
        + sizeof(std::uint32_t); // partindex

    static constexpr std::uint16_t MAX_PACKET_SIZE = 1430;
    static constexpr std::uint16_t MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE - PACKET_HEADER_SIZE;

    packet_type_enum packettype;
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
    std::int64_t filesize;
    std::int64_t offset;
};

struct file_request
{
    universal_id fileid;
    std::int64_t offset;
};

struct file_stop
{
    universal_id fileid;
};

struct file_begin
{
    universal_id fileid;
    std::int64_t offset;
};

// Used for troubleshooting only
struct file_chunk_header
{
    universal_id fileid;
    std::int64_t offset;
    chunksize_t  chunksize;
};

struct file_chunk
{
    universal_id fileid;
    std::int64_t offset;
    chunksize_t  chunksize;
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

struct message_unit
{
    message_id_t id;
    universal_id addressee;
    packet pkt;
};

/**
 * @param packet_size Maximum packet size (must be less or equal to @c packet::MAX_PACKET_SIZE and
 *        greater than @c packet::PACKET_HEADER_SIZE).
 */
template <typename QueueType>
message_id_t enqueue_packets (message_id_t msgid, QueueType & q, universal_id addresser
    , universal_id addressee, packet_type_enum packettype, std::uint16_t packet_size
    , char const * data, int len)
{
    auto payload_size = packet_size - packet::PACKET_HEADER_SIZE;
    auto remain_len   = len;
    char const * remain_data = data;
    std::uint32_t partindex  = 1;
    std::uint32_t partcount  = len / payload_size
        + (len % payload_size ? 1 : 0);

    while (remain_len) {
        packet p;
        p.packettype  = packettype;
        p.packetsize  = packet_size;
        p.addresser   = addresser;
        p.payloadsize = remain_len > payload_size
            ? payload_size
            : static_cast<std::uint16_t>(remain_len);
        p.partcount   = partcount;
        p.partindex   = partindex++;
        std::memset(p.payload, 0, payload_size);
        std::memcpy(p.payload, remain_data, p.payloadsize);

        remain_len -= p.payloadsize;
        remain_data += p.payloadsize;

        // May throw std::bad_alloc
        q.push(message_unit{msgid, addressee, std::move(p)});
    }

    return msgid;
}

#if NETTY_P2P__CEREAL_ENABLED
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
// file_chunk_header
////////////////////////////////////////////////////////////////////////////////
inline void load (cereal::BinaryInputArchive & ar, file_chunk_header & fch)
{
    ar >> fch.fileid
        >> ntoh_wrapper<decltype(fch.offset)>{fch.offset}
        >> ntoh_wrapper<decltype(fch.chunksize)>{fch.chunksize};
}

////////////////////////////////////////////////////////////////////////////////
// file_chunk
////////////////////////////////////////////////////////////////////////////////
inline void save (cereal::BinaryOutputArchive & ar, file_chunk const & fc)
{
    ar << fc.fileid
        << pfs::to_network_order(fc.offset)
        << pfs::to_network_order(fc.chunksize)
        << cereal::binary_data(fc.chunk.data(), fc.chunk.size());
}

inline void load (cereal::BinaryInputArchive & ar, file_chunk & fc)
{
    ar >> fc.fileid
        >> ntoh_wrapper<decltype(fc.offset)>{fc.offset}
        >> ntoh_wrapper<decltype(fc.chunksize)>{fc.chunksize};

        fc.chunk.resize(fc.chunksize);
        ar >> cereal::binary_data(fc.chunk.data(), fc.chunksize);
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
}

inline void load (cereal::BinaryInputArchive & ar, file_end & fe)
{
    ar >> fe.fileid;
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

#endif

}} // namespace netty::p2p
