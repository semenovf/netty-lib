////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Protocol Version 1
//
// Changelog:
//      2025.11.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <pfs/crc16.hpp>
#include <pfs/i18n.hpp>
#include <pfs/numeric_cast.hpp>
#include <cstdint>

NETTY__NAMESPACE_BEGIN

namespace pubsub {

/// Packet type
enum class packet_enum
{
      data = 1 /// Basic packet (since version 1)
};

// Byte 0:
// +-------------------------+
// | 7  6  5  4 | 3  2  1  0 |
// +-------------------------+
// |    (V)     |     (P)    |
// +------------+------------+
// (V) - Packet version (1 - first, 2 - second, etc).
// (P) - Packet type (see packet_enum).
//
// Byte 1:
// +-------------------------------+
// | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// +-------------------------------+
// | F6| F5| F4| F3| F2| F1| F0| C |
// +-------------------------------+
// (C) - Checksum bit (0 - no checksum, 1 - has checksum).
// (F0)-(F6) - free/reserved bits (can be used by some packets)

class header
{
public:
    static constexpr int VERSION = 1;

protected:
    struct {
        std::uint8_t b0;
        std::uint8_t b1;
        std::int16_t crc16;   // Optional if checksum bit is 0
        std::uint32_t length; // Mandatory for `data` packet
    } _h;

protected:
    header (packet_enum type, bool force_checksum) noexcept
    {
        std::memset(& _h, 0, sizeof(header));
        _h.b0 |= (static_cast<std::uint8_t>(VERSION) << 4) & 0xF0;
        _h.b0 |= static_cast<std::uint8_t>(type) & 0x0F;
        _h.b1 |= (force_checksum ? 0x01 : 0x00);
    }

public:
    template <typename Deserializer>
    header (Deserializer & in)
    {
        in >> _h.b0;
        in >> _h.b1;

        if (version() != VERSION) {
            throw netty::error {
                  make_error_code(netty::errc::protocol_version_error)
                , tr::f_("expected pubsub protocol version: {}, got: {}", VERSION, version())
            };
        }

        if (has_checksum())
            in >> _h.crc16;

        if (type() == packet_enum::data)
            in >> _h.length;
    }

public:
    inline int version () const noexcept
    {
        return static_cast<int>((_h.b0 >> 4) & 0x0F);
    }

    inline packet_enum type () const noexcept
    {
        return static_cast<packet_enum>(_h.b0 & 0x0F);
    }

    inline bool has_checksum () const noexcept
    {
        return static_cast<bool>(_h.b1 & 0x01);
    }

    inline bool is_f0 () const noexcept
    {
        return static_cast<bool>(_h.b1 & 0x02);
    }

    inline bool is_f1 () const noexcept
    {
        return static_cast<bool>(_h.b1 & 0x04);
    }

    inline bool is_f2 () const noexcept
    {
        return static_cast<bool>(_h.b1 & 0x08);
    }

    inline bool is_f3 () const noexcept
    {
        return static_cast<bool>(_h.b1 & 0x10);
    }

    inline bool is_f4 () const noexcept
    {
        return static_cast<bool>(_h.b1 & 0x20);
    }

    inline bool is_f5 () const noexcept
    {
        return static_cast<bool>(_h.b1 & 0x40);
    }

    inline bool is_f6 () const noexcept
    {
        return static_cast<bool>(_h.b1 & 0x80);
    }

    inline void enable_f0 ()
    {
        _h.b1 |= 0x02;
    }

    inline void enable_f1 ()
    {
        _h.b1 |= 0x04;
    }

    inline void enable_f2 ()
    {
        _h.b1 |= 0x08;
    }

    inline void enable_f3 ()
    {
        _h.b1 |= 0x10;
    }

    inline void enable_f4 ()
    {
        _h.b1 |= 0x20;
    }

    inline void enable_f5 ()
    {
        _h.b1 |= 0x40;
    }

    inline void enable_f6 ()
    {
        _h.b1 |= 0x80;
    }

protected:
    template <typename Serializer>
    void serialize (Serializer & out)
    {
        out << _h.b0 << _h.b1;

        // Has checksum
        if (has_checksum())
            out << _h.crc16;

        auto has_data = (type() == packet_enum::data);

        if (has_data)
            out << _h.length;
    }
};

constexpr int header::VERSION;

////////////////////////////////////////////////////////////////////////////////////////////////////
// data packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class data_packet: public header
{
public:
    data_packet (bool has_checksum = true) noexcept
        : header(packet_enum::data, has_checksum)
    {}

    template <typename Deserializer, typename Archive>
    data_packet (header const & h, Deserializer & in, Archive & ar)
        : header(h)
    {
        // _h.length already has been read before
        in.read(ar, _h.length);

        if (in.is_good()) {
            if (has_checksum()) {
                auto crc16 = pfs::crc16_of_ptr(ar.data(), ar.size());

                if (crc16 != _h.crc16) {
                    throw error {
                          make_error_code(netty::errc::checksum_error)
                        , tr::f_("bad CRC16 checksum for data_packet: expected 0x{:0X}, got 0x{:0X}"
                            ", data size: {} bytes"
                            , _h.crc16, crc16, ar.size())
                    };
                }
            }
        }
    }

public:
    template <typename Serializer>
    void serialize (Serializer & out, char const * data, std::size_t len)
    {
        if (has_checksum())
            _h.crc16 = pfs::crc16_of_ptr(data, len);

        _h.length = pfs::numeric_cast<decltype(_h.length)>(len);

        header::serialize(out);
        out.write(data, len);
    }
};

} // namespace pubsub

NETTY__NAMESPACE_END
