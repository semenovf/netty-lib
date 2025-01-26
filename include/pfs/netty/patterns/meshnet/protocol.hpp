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
      handshake =  1 /// Handshake phase packet
    , heartbeat =  2 /// Heartbeat loop packet
    , data      = 15 /// User data packet
};

// Used when need to specify the direction/way of the packet
enum class packet_way_enum
{
      request
    , response
};

enum class packet_category_enum
{
      notification
    , request
    , response
    , reply = response
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
// ------------------------------
// | 7  6  5  4 | 3 | 2 | 1 | 0 |
// ------------------------------
// |    (Pr)    | F2| F1| F0| C |
// ------------------------------
// (Pr) - Priority (0 - max, 7 - min).
// (C) - Checksum bit (0 - no checksum, 1 - has checksum).
// (F0), (F1), (F2) - free bits (can be used by some packets)

namespace {
    constexpr std::size_t MIN_HEADER_SIZE = 2;
}

class header
{
protected:
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

    header (header const & other)
    {
        std::memcpy(& _h, & other._h, sizeof(_h));
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
// handshake packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class handshake_packet: public header
{
public:
    std::string id;

public:
    handshake_packet (packet_way_enum way) noexcept
        : header(packet_enum::handshake, 0, false, 0)
    {
        if (way == packet_way_enum::response)
            enable_f0();
    }

    /**
     * Constructs handshake packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    explicit handshake_packet (header const & h, Deserializer & in)
        : header(h)
    {
        std::uint8_t sz = 0;
        in >> sz >> std::make_pair(& id, & sz);
    }

public:
    bool is_response () const noexcept
    {
        return is_f0();
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        if (!id.empty()) {
            header::serialize(out);
            out << std::make_pair(id.data(), pfs::numeric_cast<std::uint8_t>(id.size()));
        }
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// heartbeat packet
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

    /**
     * Constructs heartbeat packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    explicit heartbeat_packet (header const & h, Deserializer & in)
        : header(h)
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

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
