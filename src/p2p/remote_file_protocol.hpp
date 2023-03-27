////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cstdint>
#include <vector>

namespace netty {
namespace p2p {

// Protocol envelope
//
//   1        2               3            4    5
// ------------------------------------------------
// |xBF|    size    | p a y l o a d ... |crc16|xEF|
// ------------------------------------------------
// Field 1 - BEGIN flag (1 bytes, constant).
// Field 2 - Payload size (4 bytes).
// Field 3 - Payload (n bytes).
// Field 4 - CRC16 of the payload (2 bytes) (not used yet, always 0).
// Field 5 - END flag (1 byte, constant).
//
class protocol
{
    using request_id        = std::uint32_t;
    using method_field_type = std::uint8_t;
    using size_field_type   = std::uint32_t;
    using crc16_field_type  = std::int16_t;

private:
    static constexpr const char BEGIN = '\xBF';
    static constexpr const char END   = '\xEF';
    static constexpr const int MIN_PACKET_SIZE = sizeof(BEGIN)
        + sizeof(size_field_type)
        + sizeof(crc16_field_type) + sizeof(END);

private:
    request_id _rid {0}; // Request identifier

private:
    request_id next_rid () noexcept
    {
        return ++_rid;
    }

    std::vector<char> serialize_envelope (std::vector<char> const & payload);
    void process_payload (std::vector<char> const & payload);

public:
    template <typename Packet>
    std::vector<char> serialize (Packet const & p);

    bool has_complete_packet (char const * data, std::size_t len);
    std::pair<bool, std::size_t> exec (char const * data, std::size_t len);
};

}} // namespace netty::p2p
