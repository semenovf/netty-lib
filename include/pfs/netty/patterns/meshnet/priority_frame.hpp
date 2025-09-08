////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../chunk.hpp"
#include "../../error.hpp"
#include <pfs/assert.hpp>
#include <pfs/crc32.hpp>
#include <pfs/i18n.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

//
// Priority frame structure
// +----+----+----+----+-------...-----+----+----+----+----+----+
// | BE | pr |   size  |    payload    |       crc32       | ED |
// +----+----+----+----+-------...-----+----+----+----+----+----+
//
// First Byte (frame start flag):
// 0xBE
//
// pr:
// +-------------------------+
// | 7  6  5  4 | 3  2  1  0 |
// +------------+------------+
// |  reserved  |    (Pr)    |
// +-------------------------+
// (Pr) - Priority (0 - max, 7 - min).
//
// size:
// Frame payload size
//
// crc32
// CRC32 checksum of the payload
//
// Last Byte (frame end flag):
// 0xED
//

// For debugging only
#define NETTY__PF_SERIAL_FIELD_SUPPORT 0

class priority_frame
{
private:
#if NETTY__PF_SERIAL_FIELD_SUPPORT
    static constexpr std::uint16_t header_size () { return 4 + 4; } // flag + pr + serial + size
#else
    static constexpr std::uint16_t header_size () { return 4; } // flag + pr + size
#endif

    static constexpr std::uint16_t footer_size () { return 5; } // crc32 + flag
    static constexpr std::uint16_t empty_frame_size () { return header_size() + footer_size(); }
    static constexpr char begin_flag () { return static_cast<char>(0xBE); }
    static constexpr char end_flag () { return static_cast<char>(0xED); }

public:
    priority_frame ()
    {}

public:
    /**
     * Pack data into frame.
     */
    void pack (int priority, std::vector<char> & out, chunk & in, std::size_t frame_size)
    {
#if NETTY__PF_SERIAL_FIELD_SUPPORT
        static std::uint32_t serial = 0;
#endif
        frame_size = (std::min)(in.size() + empty_frame_size(), frame_size);

        PFS__THROW_UNEXPECTED(frame_size > empty_frame_size(), "Fix priority_frame::pack algorithm");

        std::uint16_t payload_size = frame_size - empty_frame_size();
        auto crc32 = pfs::crc32_of_ptr(in.data(), payload_size);

        // Size increment
        auto new_size = out.size() + frame_size;

        out.reserve(new_size);

        out.push_back(begin_flag());
        out.push_back(static_cast<char>(priority & 0x0F));

#if NETTY__PF_SERIAL_FIELD_SUPPORT
        ++serial;
        out.push_back(static_cast<char>((serial >> 24) & 0xFF));
        out.push_back(static_cast<char>((serial >> 16) & 0xFF));
        out.push_back(static_cast<char>((serial >> 8) & 0xFF));
        out.push_back(static_cast<char>(serial & 0xFF));
#endif

        // `size` in network order
        out.push_back(static_cast<char>(payload_size >> 8));
        out.push_back(static_cast<char>(payload_size & 0x00FF));

        out.insert(out.end(), in.begin(), in.begin() + payload_size);

        // CRC32 in network order
        out.push_back(static_cast<char>((crc32 >> 24) & 0xFF));
        out.push_back(static_cast<char>((crc32 >> 16) & 0xFF));
        out.push_back(static_cast<char>((crc32 >> 8) & 0xFF));
        out.push_back(static_cast<char>(crc32 & 0xFF));

        out.push_back(end_flag());

        in.erase(in.begin(), in.begin() + payload_size);
    }

public: // static
    static bool parse (std::vector<char> & out, std::vector<char> & in)
    {
        int priority = parse_header(in);

        if (priority < 0)
            return false;

#if NETTY__PF_SERIAL_FIELD_SUPPORT
        std::uint32_t serial = ((static_cast<std::uint32_t>(in[2]) << 24) & 0xFF000000)
            | (static_cast<std::uint32_t>(in[3] << 16) & 0x00FF0000)
            | (static_cast<std::uint32_t>(in[4] << 8) & 0x0000FF00)
            | (static_cast<std::uint32_t>(in[5]) & 0x000000FF);

        std::uint16_t payload_size = ((static_cast<std::uint16_t>(in[6]) << 8) & 0xFF00)
            | (static_cast<std::uint16_t>(in[7]) & 0x00FF);
#else
        std::uint16_t payload_size = ((static_cast<std::uint16_t>(in[2]) << 8) & 0xFF00)
            | (static_cast<std::uint16_t>(in[3]) & 0x00FF);
#endif

        if (in.size() < empty_frame_size() + payload_size)
            return false;

        // Index of the last byte
        auto j = empty_frame_size() + payload_size - 1;

        if (in[j] != end_flag()) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
                , tr::f_("bad end flag: expected: 0x{:0X}, got: 0x{:0X}", end_flag(), in[j])
            };
        }

        // Index of the first byte of CRC32 field
        std::uint16_t k = header_size() + payload_size;

        std::int32_t crc32_sample = ((static_cast<std::int32_t>(in[k + 0]) << 24) & 0xFF000000)
            | (static_cast<std::int32_t>(in[k + 1] << 16) & 0x00FF0000)
            | (static_cast<std::int32_t>(in[k + 2] << 8) & 0x0000FF00)
            | (static_cast<std::int32_t>(in[k + 3]) & 0x000000FF);

        std::int32_t crc32 = pfs::crc32_of_ptr(in.data() + header_size(), payload_size);

        if (crc32 != crc32_sample) {
            throw error {
                  make_error_code(netty::errc::checksum_error)
                , tr::f_("bad CRC32 checksum: expected: 0x{:0X}, got: 0x{:0X}, priority: {}"
                    ", payload_size: {} bytes"
                    , static_cast<std::uint32_t>(crc32_sample), static_cast<std::uint32_t>(crc32)
                    , priority, payload_size)
            };
        }

        out.insert(out.end(), in.begin() + header_size(), in.begin() + header_size() + payload_size);

        in.erase(in.begin(), in.begin() + empty_frame_size() + payload_size);

        return true;
    }

    template <int PriorityCount>
    static bool parse (std::array<std::vector<char>, PriorityCount> & pool, std::vector<char> & in)
    {
        int priority = parse_header(in);

        if (priority < 0)
            return false;

        if (priority >= pool.size()) {
            throw error { make_error_code(std::errc::result_out_of_range)
                , tr::f_("priority value is out of bounds: must be less then {}, got: {}"
                    , pool.size(), priority)
            };
        }

        return parse(pool[priority], in);
    }

private: // static
    static int parse_header (std::vector<char> & in)
    {
        if (in.size() < header_size() + footer_size())
            return -1;

        if (in[0] != begin_flag()) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
                , tr::f_("bad begin flag: expected: 0x{:0X}, got: 0x{:0X}", begin_flag(), in[0])
            };
        }

        auto priority = static_cast<int>(in[1] & 0x0F);
        return priority;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
