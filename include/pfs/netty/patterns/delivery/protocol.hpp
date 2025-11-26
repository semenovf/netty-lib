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
#include "../../error.hpp"
#include "../../namespace.hpp"
#include "serial_number.hpp"
#include <pfs/assert.hpp>
#include <pfs/numeric_cast.hpp>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace delivery {

/// Packet type
enum class packet_enum: std::uint8_t
{
      syn = 1
        /// Packet used to set initial value for serial number (synchronization packet).

    , ack = 2
        /// Regular message receive acknowledgement.

    , message = 3
        /// Initial regular message part with acknowledgement required.

    , part = 4
        /// Regular message part.

    , report = 5
        /// Report (message without need acknowledgement).
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
// (V) - Packet version (1 - first, 2 - second, etc).
// (P) - Packet type (see packet_enum).
//
// Bytes 1..8: (SN) - Regular message part serial number.

class header
{
protected:
    struct {
        std::uint8_t b0;
        serial_number sn;
    } _h;

protected:
    header (packet_enum type, int version = 1) noexcept
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
//
// syn_packet contains last acknowleged serial numbers
//
template <typename MessageId>
class syn_packet: public header
{
    std::uint8_t _way;
    std::vector<std::pair<MessageId, serial_number>> _snumbers;

public:
    // Request constructor
    syn_packet (std::vector<std::pair<MessageId, serial_number>> snumbers) noexcept
        : header(packet_enum::syn)
        , _way(static_cast<std::uint8_t>(syn_way_enum::request))
        , _snumbers(std::move(snumbers))
    {
        PFS__TERMINATE(!_snumbers.empty(), "serial numbers vector is empty");

        // No matter value here for _h.sn
        _h.sn = _snumbers[0].second;
    }

    // Response constructor
    syn_packet () noexcept
        : header(packet_enum::syn)
        , _way(static_cast<std::uint8_t>(syn_way_enum::response))
    {}

    template <typename Deserializer>
    syn_packet (header const & h, Deserializer & in)
        : header(h)
    {
        in >> _way;

        if (_way == static_cast<std::uint8_t>(syn_way_enum::request)) {
            if (!in.empty()) {
                std::uint8_t size = 0;
                in >> size;

                _snumbers.resize(size);

                for (int i = 0; i < size; i++)
                    in >> _snumbers[i].first >> _snumbers[i].second;
            }
        }
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

    std::size_t count () const noexcept
    {
        return _snumbers.empty() ? 1 : _snumbers.size();
    }

    std::pair<MessageId, serial_number> at (std::size_t index) const
    {
        return _snumbers.empty() ? std::make_pair(MessageId{}, sn()) : _snumbers[index];
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
        out << _way;

        if (_way == static_cast<std::uint8_t>(syn_way_enum::request)) {
            auto size = pfs::numeric_cast<std::uint8_t>(_snumbers.size());
            out << size;

            for (int i = 0; i < size; i++)
                out << _snumbers[i].first << _snumbers[i].second;
        }
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// message_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
// +-+--------+----------------+--------+----+--------+----+------------
// |H|   SN   |     msgid      |  t.sz  |p.sz|last SN |len | payload
// +-+--------+----------------+--------+----+--------+----+------------
//  1     8           16           8       4     8      4    len bytes
// |------------------------ 49 bytes ---------------------|
//
template <typename MessageId>
class message_packet: public header
{
public:
    MessageId msgid;
    std::uint64_t total_size {0};
    std::uint32_t part_size {0};
    serial_number last_sn {0};

public:
    message_packet (serial_number initial_sn) noexcept
        : header(packet_enum::message)
    {
        _h.sn = initial_sn;
    }

    /**
     * Constructs `payload` packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer, typename Archive>
    message_packet (header const & h, Deserializer & in, Archive & bytes)
        : header(h)
    {
        std::uint32_t size = 0;
        in >> msgid >> total_size >> part_size >> last_sn >> size;
        in.read(bytes, size);

        if (!in.is_good()) {
            bytes.clear();
            return;
        }
    }

public:
    template <typename Serializer>
    void serialize (Serializer & out, char const * data, std::size_t len)
    {
        auto size = pfs::numeric_cast<std::uint32_t>(len);
        header::serialize(out);

        out << msgid << total_size << part_size << last_sn << size;
        out.write(data, len);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// part_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
// +-+--------+----+------------
// |H|   SN   |len | payload
// +-+--------+----+------------
//  1     8      4   len bytes

class part_packet: public header
{
public:
    part_packet (serial_number sn) noexcept
        : header(packet_enum::part)
    {
        _h.sn = sn;
    }

    template <typename Deserializer, typename Archive>
    part_packet (header const & h, Deserializer & in, Archive & bytes)
        : header(h)
    {
        std::uint32_t size = 0;
        in >> size;
        in.read(bytes, size);

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
        auto part_size  = pfs::numeric_cast<std::uint32_t>(len);
        out << part_size;
        out.write(data, len);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// ack_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class ack_packet: public header
{
public:
    ack_packet (serial_number sn) noexcept
        : header(packet_enum::ack)
    {
        _h.sn = sn;
    }

    template <typename Deserializer>
    ack_packet (header const & h, Deserializer & /*in*/)
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
// report_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
class report_packet: public header
{
public:
    report_packet () noexcept
        : header(packet_enum::report)
    {}

    /**
     * Constructs `payload` packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer, typename Archive>
    report_packet (header const & h, Deserializer & in, Archive & bytes)
        : header(h)
    {
        std::uint32_t size = 0;
        in >> size;
        in.read(bytes, size);

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
        auto size  = pfs::numeric_cast<std::uint32_t>(len);
        out << size;
        out.write(data, len);
    }
};

} // namespace delivery

NETTY__NAMESPACE_END
