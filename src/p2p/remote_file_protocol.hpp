////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/ionik/file_provider.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace netty {
namespace p2p {

using remote_native_handle_type = std::int32_t;

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
    using request_id           = std::uint32_t;
    using operation_field_type = std::uint8_t;
    using method_field_type    = std::uint8_t;
    using size_field_type      = std::uint32_t;
    using crc16_field_type     = std::int16_t;

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

//  initiator                    executor
//    ----                          ___
//      |                            |
//      |-------open_read_only------>|
//      |<----------handle-----------|
//      |                            |
//      |---------open_write_only--->|
//      |<----------handle-----------|
//      |                            |
//      |------------close---------->|
//      |                            |
//      |-----------offset---------->|
//      |<---------filesize----------|
//      |                            |
//      |-----------set_pos--------->|
//      |                            |
//      |------------read----------->|
//      |<---------read_response-----|
//      |                            |
//      |------------write---------->|
//      |<-------write_response------|

using filesize_t = ionik::filesize_t;

enum class operation_enum: std::uint8_t {
      request      = 0x01
    , response     = 0x02
    , notification = 0x03
    , error        = 0x04
};

enum class method_enum: std::uint8_t {
      select_file        = 0x01
    , open_read_only     = 0x02
    , open_write_only    = 0x03
    , close              = 0x04
    , offset             = 0x05
    , set_pos            = 0x06
    , read               = 0x07
    , write              = 0x08
};

struct select_file_request
{};

struct select_file_response
{
    std::string uri;
    std::string display_name;
    std::string myme_type;
    filesize_t  filesize;
};

struct open_read_only_request
{
    std::string uri;
};

struct open_write_only_request
{
    std::string uri;
    ionik::truncate_enum trunc;
};

struct close_notification
{
    remote_native_handle_type h;
};

struct offset_request
{
    remote_native_handle_type h;
};

struct set_pos_notification
{
    remote_native_handle_type h;
    filesize_t offset;
};

struct read_request
{
    remote_native_handle_type h;
    filesize_t len;
};

struct write_request
{
    remote_native_handle_type h;
    std::vector<char> data;
};

struct handle_response
{
    remote_native_handle_type h;
};

struct read_response
{
    remote_native_handle_type h;
    std::vector<char> data;
};

struct write_response
{
    remote_native_handle_type h;
    filesize_t size;
};

}} // namespace netty::p2p
