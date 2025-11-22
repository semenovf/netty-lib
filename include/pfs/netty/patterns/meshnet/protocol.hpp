////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Protocol Version 1
//
// Changelog:
//      2025.01.17 Initial version.
//      2025.07.04 Changed protocol versioning.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "alive_info.hpp"
#include "route_info.hpp"
#include "../../error.hpp"
#include "../../namespace.hpp"
#include <pfs/crc32.hpp>
#include <pfs/i18n.hpp>
#include <pfs/numeric_cast.hpp>
#include <pfs/optional.hpp>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

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
        std::int32_t crc32;   // Optional if checksum bit is 0
        std::uint32_t length; // Mandatory for ddata and gdata
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
                , tr::f_("expected meshnet protocol version: {}, got: {}", VERSION, version())
            };
        }

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
        if (has_checksum())
            out << _h.crc32;

        auto has_data = (type() == packet_enum::ddata || type() == packet_enum::gdata);

        if (has_data)
            out << _h.length;
    }
};

constexpr int header::VERSION;

////////////////////////////////////////////////////////////////////////////////////////////////////
// handshake packet
////////////////////////////////////////////////////////////////////////////////////////////////////
// Bytes 2..9: Node ID

template <typename NodeId>
class handshake_packet: public header
{
    NodeId _id;

public:
    /**
     * Construct handshake packet.
     */
    handshake_packet (NodeId id, bool is_gateway, bool behind_nat, packet_way_enum way) noexcept
        : header(packet_enum::handshake, false)
        , _id(id)
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
        in >> _id;
    }

public:
    NodeId id () const noexcept
    {
        return _id;
    }

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
        out << _id;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// heartbeat packet
////////////////////////////////////////////////////////////////////////////////////////////////////
// Byte 3: Health data (unused yet)
class heartbeat_packet: public header
{
    std::uint8_t _health_data {0};

public:
    heartbeat_packet (std::uint8_t health_data) noexcept
        : header(packet_enum::heartbeat, false)
        , _health_data(health_data)
    {}

    /**
     * Constructs heartbeat packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    heartbeat_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> _health_data;
    }

public:
    std::uint8_t health_data () const noexcept
    {
        return _health_data;
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
        out << _health_data;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// alive packet
////////////////////////////////////////////////////////////////////////////////////////////////////
// Bytes 2..9: Alive Node ID
template <typename NodeId>
class alive_packet: public header
{
    alive_info<NodeId> _ainfo;

public:
    alive_packet (alive_info<NodeId> ainfo) noexcept
        : header(packet_enum::alive, false)
        , _ainfo(std::move(ainfo))
    {}

    /**
     * Constructs packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    alive_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> _ainfo.id;
    }

public:
    alive_info<NodeId> const & info () const noexcept
    {
        return _ainfo;
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
        out << _ainfo.id;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// unreachable packet
////////////////////////////////////////////////////////////////////////////////////////////////////
// Bytes 2..9  : Node ID of the responder gateway
// Bytes 10..17: Node ID of the sender
// Bytes 18..25: Node ID of the receiver
template <typename NodeId>
class unreachable_packet: public header
{
    unreachable_info<NodeId> _uinfo;

public:
    unreachable_packet (unreachable_info<NodeId> uinfo) noexcept
        : header(packet_enum::unreach, false)
        , _uinfo(std::move(uinfo))
    {}

    /**
     * Constructs packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    unreachable_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> _uinfo.gw_id >> _uinfo.sender_id >> _uinfo.receiver_id;
    }

public:
    unreachable_info<NodeId> const & info () const noexcept
    {
        return _uinfo;
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
        out << _uinfo.gw_id << _uinfo.sender_id << _uinfo.receiver_id;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// route packet
////////////////////////////////////////////////////////////////////////////////////////////////////
// Bytes 2..9  : Node ID of the initiator node
// ...
template <typename NodeId>
class route_packet: public header
{
    route_info<NodeId> _rinfo;

public:
    route_packet (packet_way_enum way, route_info<NodeId> rinfo) noexcept
        : header(packet_enum::route, false)
        , _rinfo(std::move(rinfo))
    {
        if (way == packet_way_enum::response)
            enable_f0();
    }

    template <typename Deserializer>
    route_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> _rinfo.initiator_id;

        if (is_response())
            in >> _rinfo.responder_id;

        std::uint8_t count = 0;
        in >> count;

        for (int i = 0; i < static_cast<int>(count); i++) {
            NodeId id {};
            in >> id;
            _rinfo.route.push_back(id);
        }
    }

public:
    bool is_response () const noexcept
    {
        return is_f0();
    }

    route_info<NodeId> const & info () const noexcept
    {
        return _rinfo;
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);

        out << _rinfo.initiator_id;

        if (is_response())
            out << _rinfo.responder_id;

        out << pfs::numeric_cast<std::uint8_t>(_rinfo.route.size());

        for (auto const & id: _rinfo.route)
            out << id;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// ddata packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class ddata_packet: public header
{
public:
    ddata_packet (bool has_checksum = true) noexcept
        : header(packet_enum::ddata, has_checksum)
    {}

    template <typename Deserializer, typename Archive>
    ddata_packet (header const & h, Deserializer & in, Archive & ar)
        : header(h)
    {
        // _h.length already has been read before
        in.read(ar, _h.length);

        if (in.is_good()) {
            if (has_checksum()) {
                auto crc32 = pfs::crc32_of_ptr(ar.data(), ar.size());

                if (crc32 != _h.crc32) {
                    throw error {
                          make_error_code(netty::errc::checksum_error)
                        , tr::f_("bad CRC32 checksum for ddata_packet: expected 0x{:0X}, got 0x{:0X}"
                            ", data size: {} bytes"
                            , _h.crc32, crc32, ar.size())
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
            _h.crc32 = pfs::crc32_of_ptr(data, len);

        _h.length = pfs::numeric_cast<decltype(_h.length)>(len);

        header::serialize(out);
        out.write(data, len);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// gdata packet
////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename NodeId>
class gdata_packet: public header
{
    NodeId _sender_id;
    NodeId _receiver_id;

public:
    gdata_packet (NodeId sender_id, NodeId receiver_id, bool has_checksum = true) noexcept
        : header(packet_enum::gdata, has_checksum)
        , _sender_id(sender_id)
        , _receiver_id(receiver_id)
    {}

    template <typename Deserializer, typename Archive>
    gdata_packet (header const & h, Deserializer & in, Archive & ar)
        : header(h)
    {
        in >> _sender_id >> _receiver_id;

        if (!in.is_good())
            return;

        // _h.length already has been read before
        in.read(ar, _h.length);

        if (in.is_good()) {
            if (has_checksum()) {
                auto crc32 = pfs::crc32_of_ptr(ar.data(), ar.size());

                if (crc32 != _h.crc32) {
                    throw error {
                        make_error_code(netty::errc::checksum_error)
                        , tr::f_("bad CRC32 checksum for gdata_packet: expected 0x{:0X}, got 0x{:0X}"
                            ", data size: {} bytes"
                            , _h.crc32, crc32, ar.size())
                    };
                }
            }
        }
    }

public:
    NodeId sender_id () const noexcept
    {
        return _sender_id;
    }

    NodeId receiver_id () const noexcept
    {
        return _receiver_id;
    }

    template <typename Serializer>
    void serialize (Serializer & out, char const * data, std::size_t len)
    {
        if (has_checksum())
            _h.crc32 = pfs::crc32_of_ptr(data, len);

        _h.length = pfs::numeric_cast<decltype(_h.length)>(len);

        header::serialize(out);
        out << _sender_id << _receiver_id;
        out.write(data, len);
    }
};

} // namespace meshnet

NETTY__NAMESPACE_END
