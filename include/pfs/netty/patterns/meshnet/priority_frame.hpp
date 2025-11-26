////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.04 Initial version.
//      2025.11.17 `chunk` renamed to `buffer`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../error.hpp"
#include <pfs/assert.hpp>
#include <pfs/crc32.hpp>
#include <pfs/i18n.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

//
// Priority frame structure
//                                  n   n+1  n+2  n+3  n+4  n+5
//   0    1    2    3    4     4+size-1
// +----+----+----+----+-------...-----+----+----+----+----+----+
// | BE | pr |   size  |    payload    |       crc32       | ED |
// +----+----+----+----+-------...-----+----+----+----+----+----+
//
// First Byte (frame start flag): 0xBE
//
// 'pr' (1 byte):
// +-------------------------+
// | 7  6  5  4 | 3  2  1  0 |
// +------------+------------+
// |  reserved  |    (Pr)    |
// +-------------------------+
// (Pr) - Priority (0 - max, 7 - min).
//
// 'size'    : Frame payload size (2 bytes)
// 'payload' : Frame payload ('size' bytes)
// 'crc32'   : CRC32 checksum of the payload (4 bytes)
// Last Byte (frame end flag): 0xED
//

// For debugging only
#define NETTY__PF_SERIAL_FIELD_SUPPORT 0

/**
 * Priority frame traits
 */
template <int PriorityCount, typename SerializerTraits>
class priority_frame
{
    using serializer_traits_type = SerializerTraits;
    using archive_type = typename serializer_traits_type::archive_type;
    using serializer_type = typename serializer_traits_type::serializer_type;
    using deserializer_type = typename serializer_traits_type::deserializer_type;

public:
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
    priority_frame () = delete;

public:
    /**
     * Partially pack data into frame from the source.
     *
     * @param priority Priority value.
     * @param outp Target to pack data.
     * @param inp Data source.
     * @param frame_size Maximum frame size. Must be greater than empty_frame_size().
     */
    static void pack (int priority, archive_type & outp, archive_type & inp, std::size_t frame_size)
    {
        if (inp.empty())
            return;

#if NETTY__PF_SERIAL_FIELD_SUPPORT
        static std::uint32_t serial = 0;
#endif
        frame_size = (std::min)(inp.size() + empty_frame_size(), frame_size);

        PFS__THROW_UNEXPECTED(frame_size > empty_frame_size(), "Fix priority_frame::pack algorithm");

        std::uint16_t payload_size = frame_size - empty_frame_size();
        auto crc32 = pfs::crc32_of_ptr(inp.data(), payload_size);

        serializer_type out {outp};
        out << begin_flag();
        out << static_cast<char>(priority & 0x0F);

#if NETTY__PF_SERIAL_FIELD_SUPPORT
        ++serial;
        out << serial;
#endif

        out << payload_size;
        out.write(inp.data(), payload_size);
        out << crc32;
        out << end_flag();

        inp.erase_front(payload_size);
    }

public:
    /**
     * Parses serialized frame with extracting of the payload.
     *
     * @param pool [out] Priority pool stored payloads extracted from the frame.
     * @param inp [in] Serialized frame data.
     *
     * @return @c true if frame is complete and parsed successfully, @c false if frame is incomplete.
     *
     * @throw netty::error if frame is invalid or corrupted
     */
    static bool parse (std::array<archive_type, PriorityCount> & pool, archive_type & inp)
    {
        // Incomplete frame
        if (inp.size() < header_size() + footer_size())
            return false;

        deserializer_type in {inp.data(), inp.size()};

        char byte = 0;

        in >> byte;

        if (byte != begin_flag()) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
                , tr::f_("bad begin flag: expected 0x{:0X}, got 0x{:0X}", begin_flag(), byte)
            };
        }

        in >> byte;

        auto priority = static_cast<int>(byte & 0x0F);

        if (priority >= pool.size()) {
            throw error { make_error_code(std::errc::result_out_of_range)
                , tr::f_("priority value is out of bounds: must be less then {}, got: {}"
                    , pool.size(), priority)
            };
        }

        archive_type payload;

#if NETTY__PF_SERIAL_FIELD_SUPPORT
        std::uint32_t serial = 0;
        in >> serial;
#endif

        std::uint16_t payload_size = 0;
        in >> payload_size;

        // Incomplete frame
        if (inp.size() < empty_frame_size() + payload_size)
            return false;

        in.read(payload, payload_size);

        std::int32_t crc32 = pfs::crc32_of_ptr(payload.data(), payload.size());
        std::int32_t crc32_sample = 0;
        in >> crc32_sample;

        if (crc32 != crc32_sample) {
            throw error {
                  make_error_code(netty::errc::checksum_error)
                , tr::f_("bad CRC32 checksum: expected 0x{:0X}, got 0x{:0X}, priority: {}"
                    ", payload_size: {} bytes"
                    , static_cast<std::uint32_t>(crc32_sample), static_cast<std::uint32_t>(crc32)
                    , priority, payload_size)
            };
        }

        in >> byte;

        if (byte != end_flag()) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
                , tr::f_("bad end flag: expected 0x{:0X}, got 0x{:0X}", end_flag(), byte)
            };
        }

        if (!in.is_good()) {
            throw error {
                  make_error_code(pfs::errc::unexpected_error)
                , tr::_("invalid or corrupted priority frame")
            };
        }

        pool[priority].append(payload);
        inp.erase_front(empty_frame_size() + payload_size);

        return true;
    }
};

} // namespace meshnet

NETTY__NAMESPACE_END
