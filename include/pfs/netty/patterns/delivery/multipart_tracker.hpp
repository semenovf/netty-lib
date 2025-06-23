////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../error.hpp"
#include "../../namespace.hpp"
#include "protocol.hpp"
#include "serial_number.hpp"
#include <pfs/assert.hpp>
#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

template <typename MessageId>
class multipart_tracker
{
    using message_id = MessageId;
    using clock_type = std::chrono::steady_clock;
    using time_point_type = clock_type::time_point;

private:
    message_id _msgid;
    int _priority {0};
    bool _force_checksum {false};

    std::uint32_t _part_size {0};
    serial_number _first_sn {0};
    serial_number _last_sn {0};

    std::vector<char> _payload;
    char const * _data {nullptr};
    std::size_t _size {0};

    std::vector<bool> _parts_acked;      // Parts acknowledged
    std::size_t _remain_parts_count {0}; // Number of unacknowledged parts
    std::size_t _recent_index {0};       // Index of the first not acquired part

    time_point_type _exp_timepoint;
    std::chrono::milliseconds _exp_timeout {3000}; // Expiration timeout

public:
    multipart_tracker (message_id msgid, int priority, bool force_checksum
        , std::uint32_t part_size, serial_number first_sn, char const * msg, std::size_t length
        , std::chrono::milliseconds exp_timeout = std::chrono::milliseconds{3000})
        : _msgid(msgid)
        , _priority(priority)
        , _force_checksum(force_checksum)
        , _part_size(part_size)
        , _first_sn(first_sn)
        , _data(msg)
        , _size(length)
        , _exp_timeout(exp_timeout)
    {
        init();
    }

    /**
     * Constructs tracker for message with content that will be valid until the message
     * is transmitted.
     */
    multipart_tracker (message_id msgid, int priority, bool force_checksum
        , std::uint32_t part_size, serial_number first_sn, std::vector<char> && msg
        , std::chrono::milliseconds exp_timeout = std::chrono::milliseconds{3000})
        : _msgid(msgid)
        , _priority(priority)
        , _force_checksum(force_checksum)
        , _part_size(part_size)
        , _first_sn(first_sn)
        , _payload(std::move(msg))
        , _exp_timeout(exp_timeout)
    {
        _data = _payload.data();
        _size = _payload.size();
        init();
    }

    multipart_tracker (multipart_tracker const &) = delete;
    multipart_tracker (multipart_tracker &&) = default;

    multipart_tracker & operator = (multipart_tracker const &) = delete;
    multipart_tracker & operator = (multipart_tracker &&) = default;

    ~multipart_tracker () {}

private:
    void init ()
    {
        // part_count = 1 + size() / part_size
        // last_sn    = first_sn + part_count - 1
        //            = first_sn + (1 + size() / part_size) - 1
        //            = first_sn + size() / part_size
        _last_sn = _first_sn + _size / _part_size;

        _remain_parts_count = _last_sn - _first_sn + 1;
        _parts_acked.resize(_remain_parts_count, false);
    }

    // inline std::size_t size () const noexcept
    // {
    //     return _is_static ? _static_payload.second : _dynamic_payload.size();
    // }

    inline void update_exp_timepoint () noexcept
    {
        _exp_timepoint = clock_type::now() + _exp_timeout;
    }

public:
    message_id msgid () const noexcept
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

    serial_number first_sn () const noexcept
    {
        return _first_sn;
    }

    /**
     * Returns the serial number of message last part.
     */
    serial_number last_sn () const noexcept
    {
        return _last_sn;
    }

    /**
     * Acknowledges message part delivering.
     *
     * @return @c true if the message has been delivered completely, or @c false otherwise.
     */
    bool acknowledge (serial_number sn)
    {
        auto index = sn - _first_sn;

        PFS__THROW_UNEXPECTED(index >= 0 && index < _parts_acked.size()
            , "Fix delivery::multipart_tracker algorithm");

        if (!_parts_acked[index]) {
            PFS__THROW_UNEXPECTED(_remain_parts_count > 0, "Fix delivery::multipart_tracker algorithm");
            _parts_acked[index] = true;
            _remain_parts_count--;
        }

        // Update expiration time point every acknowledgement
        update_exp_timepoint();

        return _remain_parts_count == 0;
    }

    bool is_complete () const noexcept
    {
        return _remain_parts_count == 0;
    }

    /**
     * Acquires part by serial number.
     *
     * @return @c true if part acquired.
     */
    template <typename Serializer>
    bool acquire_part (Serializer & out, serial_number sn)
    {
        if (!(sn >= _first_sn && sn <= _last_sn)) {
            throw error {
                  make_error_code(std::errc::invalid_argument)
                , "serial number is out of bounds"
            };
        }

        std::size_t index = sn - _first_sn;
        // char const * begin = _is_static ? _static_payload.first : _dynamic_payload.data();

        auto part_size = _part_size;

        // Last part, get tail part size.
        if (index == _parts_acked.size() - 1)
            part_size = _size - _part_size * index;

        if (index == 0) { // First part
            message_packet<message_id> pkt {sn};
            pkt.msgid = _msgid;
            pkt.total_size = pfs::numeric_cast<std::uint64_t>(_size);
            pkt.part_size  = _part_size;
            pkt.last_sn    = _last_sn;
            pkt.serialize(out, _data + _part_size * index, part_size);

            // Start counting the expiration time point from sending the first part
            update_exp_timepoint();
        } else {
            part_packet pkt {sn};
            pkt.serialize(out, _data + _part_size * index, part_size);
        }

        return true;
    }

    /**
     * Returns @c true if part acquired.
     */
    template <typename Serializer>
    bool acquire_part (Serializer & out)
    {
        // Message sending completed
        if (_remain_parts_count == 0)
            return false;

        std::size_t index = 0;
        bool found = false;

        // First acquire untouched parts (not acquired before).
        // First part.
        if (_recent_index < _parts_acked.size()) {
            index = _recent_index++;
            found = true;
        } else {
            if (_exp_timepoint <= clock_type::now()) {
                // Find first expired part.
                for (std::size_t i = 0; i < _recent_index; i++) {
                    if (!_parts_acked[i]) {
                        index = i;
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found)
            return false;

        auto sn = _first_sn + index;

        return acquire_part<Serializer>(out, sn);
    }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
