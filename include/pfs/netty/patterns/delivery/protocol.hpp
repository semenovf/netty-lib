////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.13 Initial version.
//      2025.04.20 Serial ID replaced by serial number.
//                 packet_enum::payload replaced by packet_enum::message.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "serial_number.hpp"
#include <pfs/netty/namespace.hpp>
#include <pfs/numeric_cast.hpp>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

/// Packet type
enum class packet_enum: std::uint8_t
{
      syn = 1
        /// Packet used to set initial value for serial number (synchronization packet).

    , message = 2
        /// Initial regular message part with acknowledgement required.

    , report = 3
        /// Initial report message part without need acknowledgement.

    , part = 4
        /// Regular message or report part.

    , ack = 5
        /// Regular message receive acknowledgement.

    , nak = 6
        /// Request the retransmission of regular message.
};

enum class syn_way_enum: std::uint8_t
{
      request
    , response
};

// Header:
//
// Byte 0:
// ---------------------------
// | 7  6  5  4 | 3  2  1  0 |
// ---------------------------
// |    (V)     |     (P)    |
// ---------------------------
// (V) - Packet version (0 - first, 1 - second, etc).
// (P) - Packet type (see packet_enum).
//
// Bytes 0..7: (SN) - Message serial number (short message identifier while transmitting).

class header
{
protected:
    struct {
        std::uint8_t b0;
        serial_number sn;
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
            in >> _h.sn;
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

    serial_number sn () const noexcept
    {
        return _h.sn;
    }

protected:
    template <typename Serializer>
    void serialize (Serializer & out)
    {
        out << _h.b0;

        if (type() != packet_enum::report)
            out << _h.sn;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// syn_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class syn_packet: public header
{
    std::uint8_t _way;

public:
    syn_packet (syn_way_enum way, serial_number sn) noexcept
        : header(packet_enum::syn, 0)
        , _way(static_cast<std::uint8_t>(way))
    {
        _h.sn = sn;
    }

    template <typename Deserializer>
    syn_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> _way;
    }

public:
    bool is_request () const noexcept
    {
        return _way == static_cast<std::uint8_t>(syn_way_enum::request);
    }

    syn_way_enum way () const noexcept
    {
        return static_cast<syn_way_enum>(_way);
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
        out << _way;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// message_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class message_packet: public header
{
public:
    std::string msgid;            // For regular message only
    std::uint64_t total_size {0}; // Message/report total size
    std::uint32_t part_size {0};
    serial_number last_sn {0};

public:
    message_packet (serial_number initial_sn) noexcept
        : header(packet_enum::message, 0)
    {
        _h.sn = initial_sn;
    }

    /**
     * Constructs `payload` packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    message_packet (header const & h, Deserializer & in, std::vector<char> & bytes)
        : header(h)
    {
        if (type() == packet_enum::message) {
            std::uint16_t msgid_size = 0;
            in >> std::make_pair(& msgid_size, & msgid);
        }

        in >> total_size >> part_size >> last_sn;

        std::uint32_t size = 0;
        in >> std::make_pair(& size, & bytes);

        if (!in.is_good()) {
            bytes.clear();
            return;
        }
    }

public:
    template <typename Serializer>
    void serialize (Serializer & out, char const * data, std::size_t len)
    {
        header::serialize(out);

        if (type() == packet_enum::message) {
            auto msgid_size = pfs::numeric_cast<std::uint16_t>(msgid.size());
            out << msgid_size << msgid;
        }

        out << total_size << part_size << last_sn;

        auto size  = pfs::numeric_cast<std::uint32_t>(len);
        out << size << std::make_pair(data, len);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// part_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class part_packet: public header
{
public:
    part_packet (serial_number sn) noexcept
        : header(packet_enum::part, 0)
    {
        _h.sn = sn;
    }

    // /**
    //  * Constructs `payload` packet from deserializer with predefined header.
    //  * Header can be read before from the deserializer.
    //  */
    // template <typename Deserializer>
    // message_packet (header const & h, Deserializer & in)
    //     : header(h)
    // {
    //     in >> std::make_pair(& bytes, & _h.length);
    //
    //     if (!in.is_good()) {
    //         bytes.clear();
    //         return;
    //     }
    // }

public:
    template <typename Serializer>
    void serialize (Serializer & out, char const * data, std::size_t len)
    {
        header::serialize(out);
        auto part_size  = pfs::numeric_cast<std::uint32_t>(len);
        out << part_size << std::make_pair(data, len);
    }

    template <typename Serializer>
    void serialize (Serializer & out, std::pair<char const *, std::size_t> data)
    {
        serialize(out, data.first, data.second);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// ack_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class ack_packet: public header
{
public:
    ack_packet (serial_number sn) noexcept
        : header(packet_enum::ack, 0)
    {
        _h.sn = sn;
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
// nak_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
// class nak_packet: public header
// {
// public:
//     nak_packet (serial_number sn) noexcept
//         : header(packet_enum::nack, 0)
//     {
//         _h.sn = sn;
//     }
//
//     /**
//      * Constructs `ack` packet from deserializer with predefined header.
//      * Header can be read before from the deserializer.
//      */
//     template <typename Deserializer>
//     nack_packet (header const & h, Deserializer & in)
//         : header(h)
//     {}
//
// public:
//     template <typename Serializer>
//     void serialize (Serializer & out)
//     {
//         header::serialize(out);
//     }
// };

// ////////////////////////////////////////////////////////////////////////////////////////////////////
// // again_packet
// ////////////////////////////////////////////////////////////////////////////////////////////////////
// class again_packet: public header
// {
// public:
//     again_packet (serial_number sn) noexcept
//         : header(packet_enum::again, 0)
//     {
//         _h.sn = sn;
//     }
//
//     /**
//      * Constructs `ack` packet from deserializer with predefined header.
//      * Header can be read before from the deserializer.
//      */
//     template <typename Deserializer>
//     again_packet (header const & h, Deserializer & in)
//         : header(h)
//     {}
//
// public:
//     template <typename Serializer>
//     void serialize (Serializer & out)
//     {
//         header::serialize(out);
//     }
// };

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
