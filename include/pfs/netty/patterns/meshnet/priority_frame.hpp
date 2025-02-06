////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/i18n.hpp>
#include <pfs/numeric_cast.hpp>
#include <pfs/optional.hpp>
#include <pfs/netty/namespace.hpp>
#include <pfs/netty/error.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

// Byte 0:
// ---------------------------
// | 7  6  5  4 | 3  2  1  0 |
// ---------------------------
// |    (M)     |    (Pr)    |
// ---------------------------
// (M)  - Magic number (0101).
// (Pr) - Priority (0 - max, 7 - min).
//
class priority_frame
{
public:
    static constexpr std::uint16_t header_size () { return 3; }

protected:
    struct
    {
        std::uint8_t b0;
        std::uint16_t size; // frame payload size
    } _h;

public:
    priority_frame ()
    {
        _h.b0 = 0;
        _h.size = 0;
    }

    priority_frame (int priority)
    {
        _h.b0 |= (static_cast<std::uint8_t>(5) << 4) & 0xF0;
        _h.b0 |= static_cast<std::uint8_t>(priority) & 0x0F;
        _h.size = 0;
    }

public:
    int priority () const noexcept
    {
        return static_cast<int>(_h.b0 & 0x0F);
    }

    std::size_t payload_size () const noexcept
    {
        return _h.size;
    }

    /**
     * Serialize frame header into vector.
     *
     * @param out Destination of serialized data.
     * @param first Start of data to be serialized.
     *
     */
    void serialize (std::vector<char> & out, char const * first, std::size_t frame_size)
    {
        _h.size = frame_size - header_size();

        // Size increment
        auto new_size = out.size() + header_size() + _h.size;

        out.reserve(new_size);

        out.push_back(static_cast<char>(_h.b0));

        // `size` in network order
        out.push_back(static_cast<char>(_h.size >> 8));
        out.push_back(static_cast<char>(_h.size & 0x00FF));

        out.insert(out.end(), first, first + _h.size);
    }

public: // static
    static pfs::optional<priority_frame> parse (char const * data, std::size_t len)
    {
        priority_frame f;

        if (len > 0)
            f._h.b0 = static_cast<std::uint8_t>(data[0]);

        if (len >= header_size()) {
            f._h.size = ((static_cast<std::uint16_t>(data[1]) << 8) & 0xFF00)
                | (static_cast<std::uint16_t>(data[2]) & 0x00FF);

            auto magic = (f._h.b0 >> 4) & 0x0F;

            if (magic != 5) {
                throw netty::error {
                      pfs::errc::unexpected_data
                    , tr::f_("bad priority frame: unexpected value of magic number({}): {}", 5, magic)
                };
            }
        }

        if (len < header_size() + f._h.size)
            return pfs::nullopt;

        return f;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
