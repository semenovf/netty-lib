////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "serial_id.hpp"
#include <pfs/netty/namespace.hpp>
#include <pfs/numeric_cast.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace reliable_delivery {

using serial_id = std::uint64_t;

/// Packet type
enum class packet_enum: std::uint8_t
{
      payload = 0
        /// Envelope payload.

    , report = 1
        /// Payload without need acknowledgement.

    , ack = 2
        /// Envelope receive acknowledgement.

    , nack = 3
        /// An envelope used to notify the sender that the payload has already been processed
        /// previously.

    , again = 4
        /// Request the retransmission of message.
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
// Bytes 1..8 : (SID) - serial ID (8 bytes), optional, not used if type is `report`
// Bytes 9..12: (L)   - payload length, optional, used by types `payload` or `report`

class header
{
protected:
    struct {
        std::uint8_t b0;
        std::uint64_t sid;
        std::uint32_t length;
    } _h;

protected:
    header (packet_enum type, int version = 0) noexcept
    {
        std::memset(& _h, 0, sizeof(header));
        _h.b0 |= (static_cast<std::uint8_t>(version) << 4) & 0xF0;
        _h.b0 |= static_cast<std::uint8_t>(type) & 0x0F;
    }

public:
    template <typename Deserializer>
    header (Deserializer & in)
    {
        in >> _h.b0;

        if (type() != packet_enum::report)
            in >> _h.sid;

        if (type() == packet_enum::payload || type() == packet_enum::report)
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

    serial_id id () const noexcept
    {
        return _h.sid;
    }

protected:
    template <typename Serializer>
    void serialize (Serializer & out)
    {
        out << _h.b0;

        if (type() != packet_enum::report)
            out << _h.sid;

        if (type() == packet_enum::payload || type() == packet_enum::report)
            out << _h.length;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// payload_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class payload_packet: public header
{
public:
    std::vector<char> bytes;   // used by deserializer only

public:
    payload_packet (serial_id sid) noexcept
        : header(packet_enum::payload, 0)
    {
        _h.sid = sid;
    }

    /**
     * Constructs `payload` packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    payload_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> std::make_pair(& bytes, & _h.length);

        if (!in.is_good()) {
            bytes.clear();
            return;
        }
    }

public:
    template <typename Serializer>
    void serialize (Serializer & out, char const * data, std::size_t len)
    {
        _h.length = pfs::numeric_cast<decltype(_h.length)>(len);
        header::serialize(out);
        out << std::make_pair(data, len);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// report_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class report_packet: public header
{
public:
    std::vector<char> bytes;   // used by deserializer only

public:
    report_packet () noexcept
        : header(packet_enum::report, 0)
    {}

    /**
     * Constructs `report` packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    report_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> std::make_pair(& bytes, & _h.length);

        if (!in.is_good()) {
            bytes.clear();
            return;
        }
    }

public:
    template <typename Serializer>
    void serialize (Serializer & out, char const * data, std::size_t len)
    {
        _h.length = pfs::numeric_cast<decltype(_h.length)>(len);
        header::serialize(out);
        out << std::make_pair(data, len);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// ack_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class ack_packet: public header
{
public:
    ack_packet (serial_id sid) noexcept
        : header(packet_enum::ack, 0)
    {
        _h.sid = sid;
    }

    /**
     * Constructs `ack` packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    ack_packet (header const & h, Deserializer & in)
        : header(h)
    {}

public:
    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// ack_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class nack_packet: public header
{
public:
    nack_packet (serial_id sid) noexcept
        : header(packet_enum::nack, 0)
    {
        _h.sid = sid;
    }

    /**
     * Constructs `ack` packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    nack_packet (header const & h, Deserializer & in)
        : header(h)
    {}

public:
    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// again_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class again_packet: public header
{
public:
    again_packet (serial_id sid) noexcept
        : header(packet_enum::again, 0)
    {
        _h.sid = sid;
    }

    /**
     * Constructs `ack` packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    again_packet (header const & h, Deserializer & in)
        : header(h)
    {}

public:
    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
    }
};

}} // namespace patterns::reliable_delivery

NETTY__NAMESPACE_END

