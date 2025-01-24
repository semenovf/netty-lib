////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/namespace.hpp>
#include <pfs/numeric_cast.hpp>
#include <pfs/optional.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

/// Packet type
enum class packet_enum
{
      discovery =  1 /// Network discovery packet (unused yet, for future purposes)
    , heartbeat =  2 /// Heartbeat loop packet
    , handshake =  3 /// Handshake phase packet
    , data      = 15 /// User data packet
};

// Byte 0:
// ---------------------------
// | 7  6  5  4 | 3  2  1  0 |
// ---------------------------
// |    (V)     |     (P)    |
// ---------------------------
// (V) - Packet version (0 - first, 1 - second, etc).
// (P) - Packet type (see packet_enum).
//
// Byte 1:
// ----------------------------
// | 7  6  5  4 | 3  2  1 | 0 |
// ----------------------------
// |    (Pr)    |reserved | C |
// ----------------------------
// (Pr) - Priority (0 - max, 7 - min).
// (C) - Checksum bit (0 - no checksum, 1 - has checksum).
//

namespace {
    constexpr std::size_t MIN_HEADER_SIZE = 2;
}

class header
{
    struct {
        std::uint8_t b0;
        std::uint8_t b1;
        std::uint32_t crc32;  // Optional if checksum bit is 0
        std::uint32_t length; // Optional if packet is a service type packet (all excluding packet_enum::data)
    } _h;

protected:
    header (packet_enum type, int priority, bool has_checksum, int version = 0) noexcept
    {
        std::memset(& _h, 0, sizeof(header));
        _h.b0 |= static_cast<std::uint8_t>(version) & 0xF0;
        _h.b0 |= static_cast<std::uint8_t>(type) & 0x0F;
        _h.b1 |= static_cast<std::uint8_t>(priority) & 0xF0;
        _h.b1 |= (has_checksum ? 0x01 : 0x00);
    }

public:
    template <typename Deserializer>
    header (Deserializer & in)
    {
        in >> _h.b0;
        in >> _h.b1;
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

    inline int priority () const noexcept
    {
        return static_cast<int>((_h.b1 >> 4) & 0x0F);
    }

    inline bool has_checksum () const noexcept
    {
        return static_cast<bool>(_h.b1 & 0x01);
    }

protected:
    template <typename Serializer>
    void serialize (Serializer & out)
    {
        out << _h.b0 << _h.b1;

        // Has checksum
        if (_h.b1 & 0x01)
            out << _h.crc32;

        if (static_cast<packet_enum>(_h.b0 & 0x0F) == packet_enum::data)
            out << _h.length;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// HeartBeat packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class heartbeat_packet: public header
{
public:
    std::uint8_t health_data;

public:
    heartbeat_packet () noexcept
        : header(packet_enum::heartbeat, 0, false, 0)
    {
        health_data = 0;
    }

    template <typename Deserializer>
    explicit heartbeat_packet (Deserializer & in)
        : heartbeat_packet()
    {
        in >> health_data;
    }

public:
    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
        out << health_data;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Handshake packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class handshake_packet: public header
{
    std::string _id;

public:
    handshake_packet () noexcept
        : header(packet_enum::handshake, 0, false, 0)
    {}

    template <typename Deserializer>
    explicit handshake_packet (Deserializer & in)
        : handshake_packet()
    {
        std::uint8_t sz = 0;
        in >> sz >> std::make_pair(& _id, & sz);
    }

public:
    template <typename U>
    void set_id (U const & value)
    {
        _id = to_string(value);
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        if (!_id.empty()) {
            header::serialize(out);
            out << std::make_pair(_id.data(), pfs::numeric_cast<std::uint8_t>(_id.size()));
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
