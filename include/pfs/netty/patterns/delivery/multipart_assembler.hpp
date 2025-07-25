////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../error.hpp"
#include "../../namespace.hpp"
#include "serial_number.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

// A - Received parts
//
// +-------------------------------------------------------------------------------------------+
// | A | A | A |   |   | A | A |   | A | A | A | A | A |   |   |   |   |   |   |   |   |   |   |
// +-------------------------------------------------------------------------------------------+
//   ^       ^                                                                               ^
//   |       |                                                                               |
//   |       +--- lowest_acked_sn()                                                          |
//   |                                                                                       |
//   +--- _first_sn                                                              _last_sn ---+

template <typename MessageId>
class multipart_assembler
{
    MessageId _msgid; // Serialized message ID
    std::uint32_t _part_size {0};
    serial_number _first_sn {0};
    serial_number _last_sn {0};

    std::vector<bool> _parts_received;
    std::vector<char> _payload;
    std::size_t _remain_parts {0};

    // For track progress
    std::size_t _received_size {0};

public:
    /**
     *  Constructs multipart message assembler.
     */
    multipart_assembler (MessageId msgid, std::uint64_t total_size, std::uint32_t part_size
        , serial_number first_sn, serial_number last_sn)
        : _msgid(msgid)
        , _part_size(part_size)
        , _first_sn(first_sn)
        , _last_sn(last_sn)
    {
        if (_last_sn < _first_sn) {
            throw error {
                  make_error_code(std::errc::invalid_argument)
                , tr::_("bad serial number bounds")
            };
        }

        _remain_parts = _last_sn - _first_sn + 1;
        _payload.resize(total_size);
        _parts_received.resize(_remain_parts, false);
    }

    multipart_assembler (multipart_assembler const &) = delete;
    multipart_assembler (multipart_assembler &&) = default;

    multipart_assembler & operator = (multipart_assembler const &) = delete;
    multipart_assembler & operator = (multipart_assembler &&) = default;

    ~multipart_assembler () {}

public:
    MessageId msgid () const noexcept
    {
        return _msgid;
    }

    /**
     * Acknowledges message part.
     *
     * @return @c false if @a sn is out of bounds.
     */
    bool acknowledge_part (serial_number sn, std::vector<char> const & part)
    {
        if (sn < _first_sn || sn > _last_sn)
            return false;

        if (part.size() > _part_size) {
            throw error {
                  make_error_code(std::errc::invalid_argument)
                , tr::_("part size too big")
            };
        }

        std::size_t index = sn - _first_sn;

        // Already received
        if (_parts_received[index])
            return true;

        PFS__THROW_UNEXPECTED(_remain_parts > 0, "Fix delivery::multipart_assembler algorithm");

        _remain_parts--;
        _received_size += part.size();

        std::copy(part.begin(), part.end(), _payload.begin() + (index * _part_size));
        _parts_received[index] = true;

        return true;
    }

    bool is_complete () const noexcept
    {
        return _remain_parts == 0;
    }

    serial_number first_sn () const noexcept
    {
        return _first_sn;
    }

    serial_number last_sn () const noexcept
    {
        return _last_sn;
    }

    serial_number lowest_acked_sn () const // noexcept
    {
        PFS__THROW_UNEXPECTED(!_parts_received.empty(), "Fix delivery::multipart_assembler algorithm");

        if (_parts_received[0]) {
            for (std::size_t i = 1; i < _parts_received.size(); i++) {
                if (!_parts_received[i])
                    return _first_sn + (i - 1);
            }

            return _first_sn;
        }

        return 0;
    }

    std::vector<char> const & payload () const noexcept
    {
        return _payload;
    }

    std::vector<char> take_payload () noexcept
    {
        auto result = std::move(_payload);
        return result;
    }

    std::size_t total_size () const noexcept
    {
        return _payload.size();
    }

    std::size_t received_size () const noexcept
    {
        return _received_size;
    }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
