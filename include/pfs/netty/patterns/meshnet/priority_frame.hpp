////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../error.hpp"
#include "../../namespace.hpp"
#include <pfs/i18n.hpp>
#include <pfs/numeric_cast.hpp>
#include <pfs/optional.hpp>
#include <pfs/c++support.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

//
// Priority frame structure
// +------------------+----+
// | B0 |   payload   | B0 |
// +------------------+----+
//
// Byte 0:
// +-------------------------+
// | 7  6  5  4 | 3  2  1  0 |
// +------------+------------+
// |    (M)     |    (Pr)    |
// +-------------------------+
// (M)  - Magic number (0101).
// (Pr) - Priority (0 - max, 7 - min).
//
// Byte 1
//
// Last Byte: Same as Byte 0
//

class priority_frame
{
public:
    static constexpr std::uint16_t header_size () { return 3; }
    static constexpr std::uint16_t footer_size () { return 1; }

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

    priority_frame (int priority) : priority_frame()
    {
        _h.b0 |= static_cast<std::uint8_t>((5 << 4) & 0xF0);
        _h.b0 |= static_cast<std::uint8_t>(priority & 0x0F);
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
     * Pack data into frame.
     */
    void pack (std::vector<char> & in, std::vector<char> & out, std::size_t frame_size)
    {
        frame_size = (std::min)(in.size() + header_size() + footer_size(), frame_size);

        _h.size = frame_size - header_size() - footer_size();

        // Size increment
        auto new_size = out.size() + frame_size;

        out.reserve(new_size);

        out.push_back(static_cast<char>(_h.b0));

        // `size` in network order
        out.push_back(static_cast<char>(_h.size >> 8));
        out.push_back(static_cast<char>(_h.size & 0x00FF));

        out.insert(out.end(), in.begin(), in.begin() + _h.size);
        out.push_back(static_cast<char>(_h.b0));

        in.erase(in.begin(), in.begin() + _h.size);
    }

public: // static
    static bool parse (std::vector<char> & in, std::vector<char> & out)
    {
        priority_frame f;

        if (in.size() > 0)
            f._h.b0 = static_cast<std::uint8_t>(in[0]);

        if (in.size() >= header_size() + footer_size()) {
            f._h.size = ((static_cast<std::uint16_t>(in[1]) << 8) & 0xFF00)
                | (static_cast<std::uint16_t>(in[2]) & 0x00FF);

            auto magic = (f._h.b0 >> 4) & 0x0F;

            if (magic != 5) {
                throw netty::error {
                      make_error_code(pfs::errc::unexpected_error)
                    , tr::f_("bad priority frame: expected value of magic number: {}, got: {}"
                        , 5, magic)
                };
            }
        }

        if (in.size() < header_size() + f._h.size + footer_size())
            return false;

        auto footer_byte = in[header_size() + f._h.size];

        if (footer_byte != f._h.b0) {
            throw netty::error { make_error_code(pfs::errc::unexpected_error)
                , tr::f_("bad priority frame: expected footer byte value: {}, got: {}"
                    , f._h.b0, footer_byte)
            };
        }

        out.insert(out.end(), in.begin() + header_size(), in.begin() + header_size() + f._h.size);
        in.erase(in.begin(), in.begin() + header_size() + f._h.size + footer_size());

        return true;
    }

    template <int PriorityCount>
    static bool parse (std::vector<char> & in, std::array<std::vector<char>, PriorityCount> & pool
        , int & priority)
    {
        priority_frame f;

        if (in.empty())
            return false;

        f._h.b0 = static_cast<std::uint8_t>(in[0]);

        if (f.priority() >= pool.size()) {
            throw netty::error { make_error_code(std::errc::result_out_of_range)
                , tr::f_("priority value is out of bounds: must be less then {}, got: {}"
                    , pool.size(), f.priority())
            };
        }

        priority = f.priority();
        return parse(in, pool[f.priority()]);
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
