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
#include "../../trace.hpp"
#include "protocol.hpp"
#include "serial_number.hpp"
#include "tag.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace delivery {

// A - acknowledged parts
// X - acquired parts (enqueued yet or sent) but not acknowledged
// Q - acquired parts (enqueued yet or sent)
//
// +-------------------------------------------------------------------------------------------+
// | A | A | A | X | X | A | A | Q | Q | Q | Q | Q | Q |   |   |   |   |   |   |   |   |   |   |
// +-------------------------------------------------------------------------------------------+
//   ^                                                   ^                                   ^
//   |                                                   |                                   |
//   |                                                   |                                   |
//   |                                                   +--- _current_index                 |
//   +--- _first_sn                                                              _last_sn ---+

template <typename MessageId, typename Archive>
class multipart_tracker
{
    using message_id = MessageId;
    using archive_type = Archive;
    using clock_type = std::chrono::steady_clock;
    using time_point_type = clock_type::time_point;

private:
    message_id _msgid;
    int _priority {0};
    bool _force_checksum {false};

    std::uint32_t _part_size {0};
    serial_number _first_sn {0}; // First value of serial number range
    serial_number _last_sn {0};  // Last value of serial number range

    archive_type _payload;
    char const * _data {nullptr};
    std::size_t _size {0};

    std::vector<bool> _parts_acked;       // Parts acknowledged
    std::vector<bool> _parts_acquired;    // Parts acqired
    std::size_t _remain_parts_count {0};  // Number of unacknowledged parts
    std::size_t _parts_acquired_count {0};
    std::size_t _current_index {0};       // Index of the first not acquired part

    time_point_type _heading_exp_timepoint;
    time_point_type _exp_timepoint;
    std::chrono::milliseconds _exp_timeout {3000}; // Expiration timeout

    // NOTE 200 is better than 500 when exchanging bitween demo programs running on host and
    // virtial box machines.
    // Maybe there is some algorithm to calculate this value.
    // Using windows size transmission may occur intermittently.
    std::size_t _window_size {200}; // Max number of parts awaiting acknowledgement

public:
    multipart_tracker (message_id msgid, int priority, std::uint32_t part_size
        , serial_number first_sn, char const * msg, std::size_t length
        , std::chrono::milliseconds exp_timeout = std::chrono::milliseconds{3000})
        : _msgid(msgid)
        , _priority(priority)
        , _part_size(part_size)
        , _first_sn(first_sn)
        , _data(msg)
        , _size(length)
        , _exp_timeout(exp_timeout)
    {
        reset();
    }

    /**
     * Constructs tracker for message with content that will be valid until the message
     * is transmitted.
     */
    multipart_tracker (message_id msgid, int priority, std::uint32_t part_size
        , serial_number first_sn, archive_type && msg
        , std::chrono::milliseconds exp_timeout = std::chrono::milliseconds{3000})
        : _msgid(msgid)
        , _priority(priority)
        , _part_size(part_size)
        , _first_sn(first_sn)
        , _payload(std::move(msg))
        , _exp_timeout(exp_timeout)
    {
        _data = _payload.data();
        _size = _payload.size();
        reset();
    }

    multipart_tracker (multipart_tracker const &) = delete;
    multipart_tracker (multipart_tracker &&) = default;

    multipart_tracker & operator = (multipart_tracker const &) = delete;
    multipart_tracker & operator = (multipart_tracker &&) = default;

    ~multipart_tracker () {}

private:
    std::size_t part_count () const noexcept
    {
        std::size_t n = _size / _part_size;
        std::size_t mod = _size % _part_size;

        return mod == 0 ? n : n + 1;
    }

    void reset ()
    {
        _remain_parts_count = part_count();
        _last_sn = _first_sn + _remain_parts_count - 1;
        _parts_acked.clear();
        _parts_acked.resize(_remain_parts_count, false);

        _parts_acquired_count = 0;
        _parts_acquired.clear();
        _parts_acquired.resize(_remain_parts_count, false);

        _current_index = 0;
        _exp_timepoint = clock_type::now();
        _heading_exp_timepoint = clock_type::now();
    }

    inline void update_exp_timepoint () noexcept
    {
        _exp_timepoint = clock_type::now() + _exp_timeout;
    }

    inline void update_heading_exp_timepoint () noexcept
    {
        _heading_exp_timepoint = clock_type::now() + _exp_timeout;
    }

    bool check_range (serial_number sn)
    {
        return sn >= _first_sn && sn <= _last_sn;
    }

    /**
     * Acquires part by serial number.
     *
     * @return @a sn.
     */
    template <typename Serializer>
    serial_number acquire_part (Serializer & out, serial_number sn)
    {
        if (!(sn >= _first_sn && sn <= _last_sn)) {
            throw error {
                  make_error_code(std::errc::invalid_argument)
                , tr::f_("serial number is out of bounds: {}: [{},{}]", sn, _first_sn, _last_sn)
            };
        }

        std::size_t index = sn - _first_sn;
        auto part_size = _part_size;
        auto is_last_part = index == _parts_acked.size() - 1;

        // Last part, calculate tail part size.
        if (is_last_part)
            part_size = _size - _part_size * index;

        if (index == 0) { // First part (message heading)
            message_packet<message_id> pkt {sn};
            pkt.msgid = _msgid;
            pkt.total_size = pfs::numeric_cast<std::uint64_t>(_size);
            pkt.part_size  = _part_size;
            pkt.last_sn    = _last_sn;
            pkt.serialize(out, _data + _part_size * index, part_size);

            // Start counting the expiration time point from sending the first part
            update_heading_exp_timepoint();
            update_exp_timepoint();
        } else {
            part_packet pkt {sn};
            pkt.serialize(out, _data + _part_size * index, part_size);
        }

        if (!_parts_acquired[index]) {
            _parts_acquired[index] = true;
            _parts_acquired_count++;
        }

        return sn;
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

    /**
     * Returns the serial number of message first part.
     */
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
     * Acknowledges delivered message part.
     *
     * @return @c false if @a sn is out of bounds.
     */
    bool acknowledge_part (serial_number sn)
    {
        if (sn < _first_sn || sn > _last_sn)
            return false;

        auto index = sn - _first_sn;

        if (!_parts_acked[index]) {
            PFS__THROW_UNEXPECTED(_remain_parts_count > 0, "Fix delivery::multipart_tracker algorithm");
            _parts_acked[index] = true;
            _remain_parts_count--;
        }

        // Update expiration time point
        update_exp_timepoint();

        return true;
    }

    bool is_complete () const noexcept
    {
        return _remain_parts_count == 0;
    }

    /**
     * Acquires next message part.
     *
     * @return Returns serial number of the next message part or zero if all parts was acquired
     *         or there are no expired parts.
     */
    template <typename Serializer>
    serial_number acquire_next_part (Serializer & out)
    {
        // Message sending is completed
        if (_remain_parts_count == 0)
            return 0;

        auto acked_parts_count = _parts_acked.size() - _remain_parts_count;

        // Check out-of-window
        if (_parts_acquired_count > acked_parts_count
                && _parts_acquired_count - acked_parts_count > _window_size) {
            return 0;
        }

        // Heading not acknowledged yet
        if (_current_index > 0 && !_parts_acked[0]) {
            if (_heading_exp_timepoint <= clock_type::now())
                _current_index = 0;

            // Acquiring message heading
            if (_current_index == 0) {
                auto sn = _first_sn;
                _current_index++;
                return acquire_part<Serializer>(out, sn);
            }

            return 0;
        }

        bool found = false;

        if (_current_index < _parts_acked.size()) {
            if (!_parts_acked[_current_index]) {
                found = true;
            } else {
                for (auto i = _current_index; i < _parts_acked.size(); i++) {
                    if (!_parts_acked[i]) {
                        _current_index = i;
                        found = true;
                        break;
                    }
                }
            }
        } else {
            // There are X-parts in the tracker
            if (_exp_timepoint <= clock_type::now()) {
                // Find first expired part.
                for (std::size_t i = 0; i < _parts_acked.size(); i++) {
                    if (!_parts_acked[i]) {
                        _current_index = i;
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found)
            return 0;

        auto sn = _first_sn + _current_index++;

        return acquire_part<Serializer>(out, sn);
    }

    // Called from delivery_controller
    void reset_to (message_id msgid, serial_number lowest_acked_sn)
    {
        if (lowest_acked_sn == 0) {
            reset();
        } else {
            if (msgid != _msgid) {
                reset();
                return;
            }

            PFS__THROW_UNEXPECTED(check_range(lowest_acked_sn)
                , tr::f_("Fix delivery::multipart_tracker algorithm:"
                    " serial number {} is out of bounds: [{},{}], msgid={}"
                        , lowest_acked_sn, _first_sn, _last_sn, to_string(_msgid)));

            std::size_t index = lowest_acked_sn - _first_sn;

            for (std::size_t i = 0; i <= index; i++)
                _parts_acked[i] = true;

            _current_index = index + 1;

            for (std::size_t i = _current_index; i < _parts_acked.size(); i++)
                _parts_acked[i] = false;

            _remain_parts_count = _parts_acked.size() - _current_index;
        }
    }
};

} // namespace delivery

NETTY__NAMESPACE_END
