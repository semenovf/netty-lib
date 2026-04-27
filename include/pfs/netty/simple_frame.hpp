////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.06 Initial version (based on patterns/pubsub/frame.hpp).
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "error.hpp"
#include <pfs/i18n.hpp>
#include <pfs/optional.hpp>
#include <algorithm>
#include <cstdint>

NETTY__NAMESPACE_BEGIN

//
// Very simple frame format
//
// +----+----+-----...-------+
// |  size   |   payload     |
// +----+----+-----...-------+
//
// Bytes 0..1      - Payload length (Size in bytes)
// Bytes 2..size+1 - Payload
//
template <typename SerializerTraits>
class simple_frame final
{
    using serializer_traits_type = SerializerTraits;
    using archive_type = typename serializer_traits_type::archive_type;
    using deserializer_type = typename serializer_traits_type::deserializer_type;
    using serializer_type = typename serializer_traits_type::serializer_type;

public:
    static constexpr std::uint16_t header_size () { return 2; }
    static constexpr std::uint16_t footer_size () { return 0; }
    static constexpr std::uint16_t empty_frame_size () { return header_size() + footer_size(); }

public:
    simple_frame () = delete;

public:
    static void pack (archive_type & outp, archive_type & inp, std::size_t frame_size)
    {
        if (inp.empty())
            return;

        frame_size = (std::min)(inp.size() + empty_frame_size(), frame_size);

        PFS__THROW_UNEXPECTED(frame_size > empty_frame_size(), "Fix pubsub::frame::pack algorithm");

        std::uint16_t payload_size = frame_size - empty_frame_size();
        serializer_type out {outp};
        out << payload_size;
        out.write(inp.data(), payload_size);

        inp.erase_front(payload_size);
    }

    static bool parse (archive_type & outp, archive_type & inp)
    {
        // Incomplete frame
        if (inp.size() < empty_frame_size())
            return false;

        deserializer_type in {inp.data(), inp.size()};
        archive_type payload;

        std::uint16_t payload_size = 0;
        in >> payload_size;

        // Incomplete frame
        if (inp.size() < empty_frame_size() + payload_size)
            return false;

        in.read(payload, payload_size);

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

NETTY__NAMESPACE_END
