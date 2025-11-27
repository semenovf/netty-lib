////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.06 Initial version (envelope.hpp).
//      2025.11.27 Moved from parent directory and renamed to frame.hpp.
//                 Fixed according to meshnet::priority_frame.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../error.hpp"
#include <pfs/i18n.hpp>
#include <pfs/optional.hpp>
#include <algorithm>
#include <cstdint>

NETTY__NAMESPACE_BEGIN

namespace pubsub {

//
// Envelope format
//
// +----+----+----+-----...-------+----+
// | BE |  size   |   payload     | ED |
// +----+----+----+-----...-------+----+
//
// Byte 0          - 0xBE, begin flag
// Bytes 1..2      - Payload length (Size bytes)
// Bytes 3..size+2 - Payload
// Byte len+3      - 0xED, end flag
//
template <typename SerializerTraits>
class frame final
{
    using serializer_traits_type = SerializerTraits;
    using archive_type = typename serializer_traits_type::archive_type;
    using deserializer_type = typename serializer_traits_type::deserializer_type;
    using serializer_type = typename serializer_traits_type::serializer_type;

public:
    static constexpr std::uint16_t header_size () { return 3; } // begin flag + size
    static constexpr std::uint16_t footer_size () { return 1; } // end flag
    static constexpr std::uint16_t empty_frame_size () { return header_size() + footer_size(); }
    static constexpr char begin_flag () { return static_cast<char>(0xBE); }
    static constexpr char end_flag () { return static_cast<char>(0xED); }

public:
    frame () = delete;

public:
    static void pack (archive_type & outp, archive_type & inp, std::size_t frame_size)
    {
        if (inp.empty())
            return;

        frame_size = (std::min)(inp.size() + empty_frame_size(), frame_size);

        PFS__THROW_UNEXPECTED(frame_size > empty_frame_size(), "Fix pubsub::frame::pack algorithm");

        std::uint16_t payload_size = frame_size - empty_frame_size();
        serializer_type out {outp};
        out << begin_flag();
        out << payload_size;
        out.write(inp.data(), payload_size);
        out << end_flag();

        inp.erase_front(payload_size);
    }

    static bool parse (archive_type & outp, archive_type & inp)
    {
        // Incomplete frame
        if (inp.size() < empty_frame_size())
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

        archive_type payload;

        std::uint16_t payload_size = 0;
        in >> payload_size;

        // Incomplete frame
        if (inp.size() < empty_frame_size() + payload_size)
            return false;

        in.read(payload, payload_size);

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
                , tr::_("invalid or corrupted frame")
            };
        }

        outp.append(payload);
        inp.erase_front(empty_frame_size() + payload_size);

        return true;
    }
};

} // namespace pubsub

NETTY__NAMESPACE_END
