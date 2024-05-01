////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.08 Initial version.
//      2021.11.17 New packet format.
//      2024.04.23 Added `ack` packet type.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "universal_id.hpp"
#include "pfs/sha256.hpp"
#include <future>
#include <sstream>
#include <cassert>
#include <cstdint>
#include <cstring>

namespace netty {
namespace p2p {

using chunksize_t = std::int32_t;

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

constexpr bool is_valid (packet_type_enum t)
{
    return t == packet_type_enum::regular
        || t == packet_type_enum::hello
        || t == packet_type_enum::file_credentials
        || t == packet_type_enum::file_request
        || t == packet_type_enum::file_chunk
        || t == packet_type_enum::file_begin
        || t == packet_type_enum::file_end
        || t == packet_type_enum::file_state
        || t == packet_type_enum::file_stop;
}


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
struct hello
{
    char greeting[4];
};

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

/**
 * @param packet_size Maximum packet size (must be less or equal to @c packet::MAX_PACKET_SIZE and
 *        greater than @c packet::PACKET_HEADER_SIZE).
 */
template <typename QueueType>
void enqueue_packets (QueueType & q, universal_id addresser
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
        q.push(std::move(p));
    }
}

}} // namespace netty::p2p
