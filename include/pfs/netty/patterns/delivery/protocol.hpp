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

namespace patterns {
namespace delivery {

/// Packet type
enum class packet_enum: std::uint8_t
{
      syn = 1
        /// Packet used to set initial value for serial number (synchronization packet).

    , ack = 2
        /// Regular message receive acknowledgement.

    , nak = 3
        /// Request the retransmission of regular message.

    , message = 4
        /// Initial regular message part with acknowledgement required.

    , part = 5
        /// Regular message part.

    , report = 6
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
// (V) - Packet version (0 - first, 1 - second, etc).
// (P) - Packet type (see packet_enum).
//
// Bytes 0..7: (SN) - Regular message part serial number.

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
    std::vector<serial_number> _snumbers;

public:
    syn_packet (syn_way_enum way, std::vector<serial_number> snumbers) noexcept
        : header(packet_enum::syn, 0)
        , _way(static_cast<std::uint8_t>(way))
        , _snumbers(std::move(snumbers))
    {
        PFS__TERMINATE(!_snumbers.empty(), "serial numbers vector is empty");
        _h.sn = _snumbers[0];

        if (_snumbers.size() == 1)
            _snumbers.clear();
    }

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

        if (!in.empty()) {
            std::uint8_t size = 0;
            in >> size;

            _snumbers.resize(size);

            for (int i = 0; i < size; i++)
                in >> _snumbers[i];
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

    std::size_t sn_count () const noexcept
    {
        return _snumbers.empty() ? 1 : _snumbers.size();
    }

    serial_number sn_at (std::size_t index) const
    {
        return _snumbers.empty() ? sn() : _snumbers[index];
    }

    template <typename Serializer>
    void serialize (Serializer & out)
    {
        header::serialize(out);
        out << _way;

        if (_snumbers.size() > 1) {
            auto size = pfs::numeric_cast<std::uint8_t>(_snumbers.size());
            out << size;

            for (int i = 0; i < size; i++)
                out << _snumbers[i];
        }
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// message_packet
////////////////////////////////////////////////////////////////////////////////////////////////////
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
        in >> msgid >> total_size >> part_size >> last_sn;

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

        out << msgid << total_size << part_size << last_sn;

        auto size = pfs::numeric_cast<std::uint32_t>(len);
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

    template <typename Deserializer>
    part_packet (header const & h, Deserializer & in, std::vector<char> & bytes)
        : header(h)
    {
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
        auto part_size  = pfs::numeric_cast<std::uint32_t>(len);
        out << part_size << std::make_pair(data, len);
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
class nak_packet: public header
{
public:
    nak_packet (serial_number sn) noexcept
        : header(packet_enum::nak, 0)
    {
        _h.sn = sn;
    }

    template <typename Deserializer>
    nak_packet (header const & h, Deserializer & in)
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
        : header(packet_enum::report, 0)
    {}

    /**
     * Constructs `payload` packet from deserializer with predefined header.
     * Header can be read before from the deserializer.
     */
    template <typename Deserializer>
    report_packet (header const & h, Deserializer & in, std::vector<char> & bytes)
        : header(h)
    {
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
        auto size  = pfs::numeric_cast<std::uint32_t>(len);
        out << size << std::make_pair(data, len);
    }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
