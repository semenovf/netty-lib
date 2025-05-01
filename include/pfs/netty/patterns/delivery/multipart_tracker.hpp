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
#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

class multipart_tracker
{
    using clock_type = std::chrono::steady_clock;
    using time_point_type = clock_type::time_point;

private:
    std::string _msgid; // Serialized message ID (for message only, empty for report)
    int _priority {0};
    bool _force_checksum {false};

    std::uint32_t _part_size {0};
    serial_number _first_sn {0};
    serial_number _last_sn {0};

    // NOTE std::variant can be used here
    bool _is_static {false};
    std::vector<char> _dynamic_payload;
    std::pair<char const *, std::size_t> _static_payload {nullptr, 0};

    std::vector<bool> _parts_acked;              // Parts acknowledged
    std::vector<time_point_type> _parts_expired; // Expiration time for parts
    std::size_t _remain_parts {0};               // Number of unacknowledged parts
    std::size_t _recent_index {0};               // Index of the first not acquired part

    std::chrono::milliseconds _exp_timeout {3000}; // Expiration timeout

public:
    /**
     *  Constructs tracker for report message.
     */
    multipart_tracker (int priority, bool force_checksum, std::uint32_t part_size
        , serial_number first_sn, std::vector<char> && msg)
        : _priority(priority)
        , _force_checksum(force_checksum)
        , _part_size(part_size)
        , _first_sn(first_sn)
        , _is_static(false)
        , _dynamic_payload(std::move(msg))
    {
        // part_count = 1 + size() / part_size
        // last_sn    = first_sn + part_count - 1
        //            = first_sn + (1 + size() / part_size) - 1
        //            = first_sn + size() / part_size
        _last_sn = _first_sn + size() / _part_size;

        _remain_parts = _last_sn - _first_sn + 1;
        _parts_acked.resize(_remain_parts, false);
        _parts_expired.resize(_remain_parts);
    }

    /**
     * Constructs tracker for report message with content that will be valid until the message
     * is transmitted.
     */
    multipart_tracker (int priority, bool force_checksum, std::uint32_t part_size
        , serial_number first_sn, char const * msg, std::size_t length)
        : _priority(priority)
        , _force_checksum(force_checksum)
        , _part_size(part_size)
        , _first_sn(first_sn)
        , _is_static(true)
        , _static_payload(msg, length)
    {}

    /**
     *  Constructs tracker for regular message.
     */
    multipart_tracker (std::string && msgid, int priority, bool force_checksum
        , std::uint32_t part_size, serial_number first_sn, std::vector<char> && msg
        , std::chrono::milliseconds exp_timeout = std::chrono::milliseconds{3000})
        : multipart_tracker(priority, force_checksum, part_size, first_sn, std::move(msg))
    {
        _msgid = std::move(msgid);
        _exp_timeout = exp_timeout;
    }

    /**
     * Constructs tracker for regular message with content that will be valid until the message
     * is transmitted.
     */
    multipart_tracker (std::string && msgid, int priority, bool force_checksum
        , std::uint32_t part_size, serial_number first_sn, char const * msg, std::size_t length
        , std::chrono::milliseconds exp_timeout = std::chrono::milliseconds{3000})
        : multipart_tracker(priority, force_checksum, part_size, first_sn, msg, length)
    {
        _msgid = std::move(msgid);
        _exp_timeout = exp_timeout;
    }

// private: // static
//     static std::pair<char const *, std::size_t> invalid_part ()
//     {
//         return std::make_pair<char const *, std::size_t>(nullptr, 0);
//     }

private:
    std::size_t size () const noexcept
    {
        return _is_static ? _static_payload.second : _dynamic_payload.size();
    }

public:
    std::string const & msgid () const noexcept
    {
        return _msgid;
    }

    int priority () const noexcept
    {
        return _priority;
    }

    bool force_checksum () const noexcept
    {
        return _force_checksum;
    }

    /**
     * Returns the serial number of message last part.
     */
    serial_number last_sn () const noexcept
    {
        return _last_sn;
    }

    bool is_report () const noexcept
    {
        return _msgid.empty();
    }

    void acknowledge (serial_number sn)
    {
        auto index = sn - _first_sn;
        PFS__TERMINATE(index >= 0 && index < _parts_acked.size()
            , "Fix meshnet::multipart_tracker algorithm");

        if (!_parts_acked[index]) {
            PFS__TERMINATE(_remain_parts > 0, "Fix meshnet::multipart_tracker algorithm");
            _parts_acked[index] = true;
            _remain_parts--;
        }
    }

    bool is_complete () const noexcept
    {
        return _remain_parts == 0;
    }

    /**
     * Returns @c true if part acquired.
     */
    template <typename Serializer>
    bool acquire_part (Serializer & out)
    {
        // Message sending completed
        if (_remain_parts == 0)
            return false;

        auto part_size = _part_size;
        std::size_t index = 0;
        bool found = false;

        // First acquire untouched parts (not acquired before).
        // First part.
        if (_recent_index < _parts_acked.size()) {
            index = _recent_index++;
            found = true;
        } else {
            if (!is_report()) {
                // Find first expired part.
                for (std::size_t i = 0; i < _recent_index; i++) {
                    if (!_parts_acked[i] && _parts_expired[i] <= clock_type::now()) {
                        index = i;
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found)
            return false;

        // Last part, get tail part size.
        if (index == _parts_acked.size() - 1)
            part_size = size() - _part_size * index;

        char const * begin = _is_static ? _static_payload.first : _dynamic_payload.data();
        auto sn = _first_sn + index;

        PFS__TERMINATE(sn >= _first_sn && sn <= _last_sn, "Fix meshnet::multipart_tracker algorithm");

        if (index == 0) { // First part
            message_packet pkt {sn};
            pkt.msgid = _msgid; // Empty string if report
            pkt.total_size = pfs::numeric_cast<std::uint64_t>(size());
            pkt.part_size  = _part_size;
            pkt.last_sn    = _last_sn;
            pkt.serialize(out, begin + _part_size * index,  part_size);
        } else {
            part_packet pkt {sn};
            pkt.serialize(out, begin + _part_size * index,  part_size);
        }

        _parts_expired[index] = clock_type::now() + _exp_timeout;

        return true;
    }

public: // static
    /**
     * Searches multipart tracker in range from @a begin to @a end (excluding) that contains
     * specified serial number @a sn.
     */
    template <typename ForwardIt>
    static ForwardIt find (ForwardIt begin, ForwardIt end, serial_number sn)
    {
        for (auto pos = begin; pos != end; ++pos) {
            if (sn >= pos->_first_sn && sn <= pos->_last_sn)
                return pos;
        }

        return end;
    }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
