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
#include <pfs/crc32.hpp>
#include <pfs/numeric_cast.hpp>
#include <pfs/optional.hpp>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

/// Packet type
enum class packet_enum
{
      handshake =  1 /// Handshake phase packet
    , heartbeat =  2 /// Heartbeat loop packet
    , route     =  3 /// Route discovery packet
    , ddata     = 14 /// User data packet for exchange inside domestic subnet (domestic data)
    , fdata     = 15 /// User data packet for exchange bitween subnets using router nodes (foreign data)
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

enum class behind_nat_enum
{
      no
    , yes
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
// | (reserved) | F2| F1| F0| C |
// ------------------------------
// (C) - Checksum bit (0 - no checksum, 1 - has checksum).
// (F0), (F1), (F2) - free/reserved bits (can be used by some packets)

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
    header (packet_enum type, bool has_checksum, int version = 0) noexcept
    {
        std::memset(& _h, 0, sizeof(header));
        _h.b0 |= (static_cast<std::uint8_t>(version) << 4) & 0xF0;
        _h.b0 |= static_cast<std::uint8_t>(type) & 0x0F;
        _h.b1 |= (has_checksum ? 0x01 : 0x00);
    }

public:
    template <typename Deserializer>
    header (Deserializer & in)
    {
        in >> _h.b0;
        in >> _h.b1;

        if (has_checksum())
            in >> _h.crc32;

        if (type() == packet_enum::ddata || type() == packet_enum::fdata)
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

        auto has_data = static_cast<packet_enum>(_h.b0 & 0x0F) == packet_enum::ddata
            || static_cast<packet_enum>(_h.b0 & 0x0F) == packet_enum::fdata;

        if (has_data)
            out << _h.length;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// handshake packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class handshake_packet: public header
{
public:
    std::pair<std::uint64_t, std::uint64_t> id;

public:
    handshake_packet (packet_way_enum way, behind_nat_enum behind_nat = behind_nat_enum::no) noexcept
        : header(packet_enum::handshake, false, 0)
    {
        if (way == packet_way_enum::response)
            enable_f0();

        if (behind_nat == behind_nat_enum::yes)
            enable_f1();
    }

    /**
     * Constructs handshake packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    handshake_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> id.first >> id.second;
    }

public:
    bool is_response () const noexcept
    {
        return is_f0();
    }

    bool is_behind_nat () const noexcept
    {
        return is_f1();
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
        out << id.first << id.second;
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
        : header(packet_enum::heartbeat, false, 0)
    {
        health_data = 0;
    }

    /**
     * Constructs heartbeat packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    heartbeat_packet (header const & h, Deserializer & in)
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

////////////////////////////////////////////////////////////////////////////////////////////////////
// route packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class route_packet: public header
{
public:
    // first - high part, seconf - low part
    std::vector<std::pair<std::uint64_t, std::uint64_t>> route; // router IDs

public:
    route_packet (packet_way_enum way) noexcept
        : header(packet_enum::route, false, 0)
    {
        if (way == packet_way_enum::response)
            enable_f0();
    }

    template <typename Deserializer>
    route_packet (header const & h, Deserializer & in)
        : header(h)
    {
        std::uint8_t count = 0;
        in >> count;

        for (int i = 0; i < static_cast<int>(count); i++) {
            std::uint64_t id_high = 0;
            std::uint64_t id_low = 0;
            in >> id_high >> id_low;
            route.push_back(std::make_pair(id_high, id_low));
        }
    }

public:
    bool is_response () const noexcept
    {
        return is_f0();
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        if (!route.empty()) {
            header::serialize(out);
            out << pfs::numeric_cast<std::uint8_t>(route.size());

            for (auto const & id: route)
                out << id.first << id.second;
        }
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// ddata packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class ddata_packet: public header
{
public:
    std::vector<char> bytes;   // Used by deserializer only
    bool bad_checksum {false}; // Used by deserializer only

public:
    ddata_packet (bool has_checksum) noexcept
        : header(packet_enum::ddata, has_checksum, 0)
    {}

    template <typename Deserializer>
    ddata_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> std::make_pair(& bytes, & _h.length);

        if (!in.is_good()) {
            bytes.clear();
            return;
        }

        if (has_checksum()) {
            auto crc32 = pfs::crc32_of_ptr(bytes.data(), bytes.size());

            if (crc32 != _h.crc32) {
                bytes.clear();
                bad_checksum = true;
            }
        }
    }

public:
    template <typename Serializer>
    void serialize (Serializer & out, char const * data, std::size_t len)
    {
        if (has_checksum())
            _h.crc32 = pfs::crc32_of_ptr(data, len);

        _h.length = pfs::numeric_cast<decltype(_h.length)>(len);

        header::serialize(out);
        out << std::make_pair(data, len);
    }

    template <typename Serializer>
    void serialize (Serializer & out, std::vector<char> && data)
    {
        serialize<Serializer>(out, data.data(), data.size());
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// fdata packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class fdata_packet: public header
{
public:
    std::pair<std::uint64_t, std::uint64_t> sender_id;
    std::pair<std::uint64_t, std::uint64_t> receiver_id;
    std::vector<char> bytes;   // Used by deserializer only
    bool bad_checksum {false}; // Used by deserializer only

public:
    fdata_packet (std::pair<std::uint64_t, std::uint64_t> sender_id
        , std::pair<std::uint64_t, std::uint64_t> receiver_id
        , bool has_checksum) noexcept
        : header(packet_enum::fdata, has_checksum, 0)
        , sender_id(std::move(sender_id))
        , receiver_id(std::move(receiver_id))
    {}

    template <typename NodeIdTraits>
    fdata_packet (typename NodeIdTraits::node_id sender_id
        , typename NodeIdTraits::node_id receiver_id
        , bool has_checksum) noexcept
        : fdata_packet(
              std::make_pair(NodeIdTraits::high(sender_id), NodeIdTraits::low(sender_id))
            , std::make_pair(NodeIdTraits::high(receiver_id), NodeIdTraits::low(receiver_id))
            , has_checksum)
    {}

    template <typename Deserializer>
    fdata_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> sender_id.first >> sender_id.second;

        if (!in.is_good())
            return;

        in >> receiver_id.first >> receiver_id.second;

        if (!in.is_good())
            return;

        in >> std::make_pair(& bytes, & _h.length);

        if (!in.is_good()) {
            bytes.clear();
            return;
        }

        if (has_checksum()) {
            auto crc32 = pfs::crc32_of_ptr(bytes.data(), bytes.size());

            if (crc32 != _h.crc32) {
                bytes.clear();
                bad_checksum = true;
            }
        }
    }

public:
    template <typename Serializer>
    void serialize (Serializer & out, char const * data, std::size_t len)
    {
        if (has_checksum())
            _h.crc32 = pfs::crc32_of_ptr(data, len);

        _h.length = pfs::numeric_cast<decltype(_h.length)>(len);

        header::serialize(out);
        out << sender_id.first << sender_id.second << receiver_id.first << receiver_id.second;
        out << std::make_pair(data, len);
    }

    template <typename Serializer>
    void serialize (Serializer & out, std::vector<char> && data)
    {
        serialize<Serializer>(out, data.data(), data.size());
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
