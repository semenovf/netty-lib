////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "alive_info.hpp"
#include "route_info.hpp"
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
    , alive     =  3 /// Alive packet (periodic)
    , unreach   =  4 /// Node is unreachable packet
    , route     =  5 /// Route discovery packet
    , ddata     = 14 /// User data packet for exchange inside domestic subnet (domestic message)
    , gdata     = 15 /// User data packet for exchange bitween subnets using router nodes (global message).
};

// Used when need to specify the direction/way of the packet
enum class packet_way_enum
{
      request
    , response
};

// Byte 0:
// +-------------------------+
// | 7  6  5  4 | 3  2  1  0 |
// +-------------------------+
// |    (V)     |     (P)    |
// +------------+------------+
// (V) - Packet version (0 - first, 1 - second, etc).
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

        if (type() == packet_enum::ddata || type() == packet_enum::gdata)
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
        if (_h.b1 & 0x01)
            out << _h.crc32;

        auto has_data = static_cast<packet_enum>(_h.b0 & 0x0F) == packet_enum::ddata
            || static_cast<packet_enum>(_h.b0 & 0x0F) == packet_enum::gdata;

        if (has_data)
            out << _h.length;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// handshake packet
////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename NodeId>
class handshake_packet: public header
{
public:
    NodeId id;

public:
    /**
     * Construct handshake packet.
     */
    handshake_packet (bool is_gateway, bool behind_nat, packet_way_enum way = packet_way_enum::request) noexcept
        : header(packet_enum::handshake, false, 0)
    {
        if (way == packet_way_enum::response)
            enable_f0();

        if (is_gateway)
            enable_f1();

        if (behind_nat)
            enable_f2();
    }

    /**
     * Constructs handshake packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    handshake_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> id;
    }

public:
    bool is_response () const noexcept
    {
        return is_f0();
    }

    bool is_gateway () const noexcept
    {
        return is_f1();
    }

    bool behind_nat () const noexcept
    {
        return is_f2();
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
        out << id;
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
// alive packet
////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename NodeId>
class alive_packet: public header
{
public:
    alive_info<NodeId> ainfo;

public:
    alive_packet () noexcept
        : header(packet_enum::alive, false, 0)
    {}

    /**
     * Constructs packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    alive_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> ainfo.id;
    }

public:
    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
        out << ainfo.id;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// alive packet
////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename NodeId>
class unreachable_packet: public header
{
public:
    unreachable_info<NodeId> uinfo;

public:
    unreachable_packet () noexcept
        : header(packet_enum::unreach, false, 0)
    {}

    /**
     * Constructs packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    unreachable_packet (header const & h, Deserializer & in)
        : header(h)
    {
        std::uint8_t gw_id_size = 0, sender_id_size = 0, receiver_id_size = 0;

        in >> uinfo.gw_id >> uinfo.sender_id >> uinfo.receiver_id;
    }

public:
    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
        out << uinfo.gw_id << uinfo.sender_id << uinfo.receiver_id;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// route packet
////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename NodeId>
class route_packet: public header
{
public:
    route_info<NodeId> rinfo;

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
        in >> rinfo.initiator_id;

        if (is_response())
            in >> rinfo.responder_id;

        std::uint8_t count = 0;
        in >> count;

        for (int i = 0; i < static_cast<int>(count); i++) {
            NodeId id {};
            in >> id;
            rinfo.route.push_back(id);
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
        header::serialize(out);

        out << rinfo.initiator_id;

        if (is_response())
            out << rinfo.responder_id;

        out << pfs::numeric_cast<std::uint8_t>(rinfo.route.size());

        for (auto const & id: rinfo.route)
            out << id;
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
        in >> std::make_pair(& bytes, std::cref(_h.length));

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
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// gdata packet
////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename NodeId>
class gdata_packet: public header
{
public:
    NodeId sender_id;
    NodeId receiver_id;
    std::vector<char> bytes;   // Used by deserializer only and when need to forward message.
    bool bad_checksum {false}; // Used by deserializer only

public:
    gdata_packet (NodeId sender_id, NodeId receiver_id, bool has_checksum) noexcept
        : header(packet_enum::gdata, has_checksum, 0)
        , sender_id(sender_id)
        , receiver_id(receiver_id)
    {}

    template <typename Deserializer>
    gdata_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> sender_id;

        if (!in.is_good())
            return;

        in >> receiver_id;

        if (!in.is_good())
            return;

        in >> std::make_pair(& bytes, std::cref(_h.length));

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
        out << sender_id << receiver_id;
        out << std::make_pair(data, len);
    }

    template <typename Serializer>
    void serialize (Serializer & out, std::vector<char> && data)
    {
        serialize<Serializer>(out, data.data(), data.size());
    }

    /**
     * This serializer used when need to forward message.
     * Expected packet is already initialized.
     */
    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
        out << sender_id << receiver_id;
        out << std::make_pair(bytes.data(), bytes.size());
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
