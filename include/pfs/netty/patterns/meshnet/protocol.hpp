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
struct header
{
    std::uint8_t b0;
    std::uint8_t b1;
    std::uint32_t crc32;  // Optional if checksum bit is 0
    std::uint32_t length; // Optional if packet is a service type packet (all excluding packet_enum::data)
};

namespace {
    constexpr std::size_t MIN_HEADER_SIZE = 2;
}

class header_constructor
{
    header & _h;

protected:
    header_constructor (header & h, packet_enum type, int priority, bool has_checksum, int version = 0)
        : _h(h)
    {
        std::memset(& _h, 0, sizeof(header));
        _h.b0 |= static_cast<std::uint8_t>(version) & 0xF0;
        _h.b0 |= static_cast<std::uint8_t>(packet_enum::handshake) & 0x0F;
        _h.b1 |= static_cast<std::uint8_t>(priority) & 0xF0;
        _h.b1 |= (has_checksum ? 0x01 : 0x00);
    }

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

public:
    int priority () const noexcept
    {
        return static_cast<int>((_h.b1 >> 4) & 0x0F);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// HeartBeat packet
////////////////////////////////////////////////////////////////////////////////////////////////////
struct heartbeat_packet
{
    header h;
    std::uint8_t health_data;
};

class heartbeat_packet_builder: public header_constructor
{
    heartbeat_packet _p;

public:
    heartbeat_packet_builder () noexcept
        : header_constructor(_p.h, packet_enum::heartbeat, 0, false, 0)
    {
        _p.health_data = 0;
    }

public:
    void set_health_data (std::uint8_t value)
    {
        _p.health_data = value;
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header_constructor::serialize(out);
        out << _p.health_data;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Handshake packet
////////////////////////////////////////////////////////////////////////////////////////////////////
struct handshake_packet
{
    header h;
    std::uint8_t uuid_size;
    std::string uuid;
};

class handshake_packet_builder: public header_constructor
{
    handshake_packet _p;

public:
    handshake_packet_builder () noexcept
        : header_constructor(_p.h, packet_enum::handshake, 0, false, 0)
    {
        _p.uuid_size = 0;
    }

public:
    template <typename U>
    void set_uuid (U const & value)
    {
        _p.uuid = to_string(value);
        _p.uuid_size = static_cast<std::uint8_t>(_p.uuid.size());
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header_constructor::serialize(out);
        out << _p.uuid_size;

        if (_p.uuid_size > 0)
            out << _p.uuid;
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
