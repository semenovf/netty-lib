////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "protocol.hpp"
#include "serial_number.hpp"
#include <cstdint>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

class multipart_tracker
{
    std::string _msgid; // Serialized message ID (for message only)
    int _priority {0};
    bool _force_checksum {false};

    std::uint32_t _part_size {0};
    serial_number _initial_sn {0};
    serial_number _last_sn {0};

    // NOTE std::variant can be used here
    bool _is_static {false};
    std::vector<char> _dynamic_payload;
    std::pair<char const *, std::size_t> _static_payload {nullptr, 0};

public:
    /**
     *  Constructs tracker for report message.
     */
    multipart_tracker (int priority, bool force_checksum, std::uint32_t part_size
        , serial_number initial_sn, std::vector<char> && msg)
        : _priority(priority)
        , _force_checksum(force_checksum)
        , _part_size(part_size)
        , _initial_sn(initial_sn)
        , _is_static(false)
        , _dynamic_payload(std::move(msg))
    {
        // part_count = 1 + size() / part_size
        // last_sn    = initial_sn + part_count - 1
        //            = initial_sn + (1 + size() / part_size) - 1
        //            = initial_sn + size() / part_size
        _last_sn = _initial_sn + size() / _part_size;
    }

    /**
     * Constructs tracker for report message with content that will be valid until the message
     * is transmitted.
     */
    multipart_tracker (int priority, bool force_checksum, std::uint32_t part_size
        , serial_number initial_sn, char const * msg, std::size_t length)
        : _priority(priority)
        , _force_checksum(force_checksum)
        , _part_size(part_size)
        , _initial_sn(initial_sn)
        , _is_static(true)
        , _static_payload(msg, length)
    {}

    /**
     *  Constructs tracker for regular message.
     */
    multipart_tracker (std::string && msgid, int priority, bool force_checksum
        , std::uint32_t part_size, serial_number initial_sn, std::vector<char> && msg)
        : multipart_tracker(priority, force_checksum, part_size, initial_sn, std::move(msg))
    {
        _msgid = std::move(msgid);
    }

    /**
     * Constructs tracker for regular message with content that will be valid until the message
     * is transmitted.
     */
    multipart_tracker (std::string && msgid, int priority, bool force_checksum
        , std::uint32_t part_size, serial_number initial_sn, char const * msg, std::size_t length)
        : multipart_tracker(priority, force_checksum, part_size, initial_sn, msg, length)
    {
        _msgid = std::move(msgid);
    }

private: // static
    static std::pair<char const *, std::size_t> invalid_part ()
    {
        return std::make_pair<char const *, std::size_t>(nullptr, 0);
    }

private:
    std::size_t size () const noexcept
    {
        return _is_static ? _static_payload.second : _dynamic_payload.size();
    }

    /**
     * Get part by serial number @a sn
     */
    std::pair<char const *, std::size_t> part (serial_number sn) const
    {
        // Check bounds
        if (sn < _initial_sn || sn > _last_sn)
            return invalid_part();

        auto part_index = sn - _initial_sn;
        char const * begin = _is_static
            ? _static_payload.first
            : _dynamic_payload.data();

        return std::make_pair(begin + part_index * _part_size, _part_size);
    }

public:
    /**
     * Returns the serial number of message first part.
     */
    serial_number initial_sn () const noexcept
    {
        return _initial_sn;
    }

    /**
     * Returns the serial number of message last part.
     */
    serial_number last_sn () const noexcept
    {
        return _last_sn;
    }

    template <typename Serializer>
    void serialize (Serializer & out, serial_number sn) const
    {
        PFS__TERMINATE(sn >= _initial_sn && sn <= _last_sn, "Fix meshnet::multipart_tracker algorithm");

        auto part_data = part(sn);

        if (sn == _initial_sn) { // First part
            message_packet pkt {sn};
            pkt.msgid = _msgid;
            pkt.total_size = pfs::numeric_cast<std::uint64_t>(size());
            pkt.part_size  = _part_size;
            pkt.initial_sn = _initial_sn;
            pkt.last_sn    = _last_sn;
            pkt.serialize(out, part_data);
        } else {
            part_packet pkt {sn};
            pkt.serialize(out, part_data);
        }
    }

public: // static
    /**
     * Searches multipart tracker in range from @a begin to @a end (excluding) that contains
     * specified serial number @a sn.
     */
    template <typename ForwardIt>
    static ForwardIt find (serial_number sn, ForwardIt begin, ForwardIt end)
    {
        for (auto pos = begin; pos != end; ++pos) {
            if (sn >= pos->_initial_sn && sn <= pos->_last_sn)
                return pos;
        }

        return end;
    }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
