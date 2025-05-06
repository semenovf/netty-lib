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
#include "protocol.hpp"
#include "serial_number.hpp"
#include <pfs/assert.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

class multipart_assembler
{
    std::string _msgid; // Serialized message ID
    std::uint32_t _part_size {0};
    serial_number _first_sn {0};
    serial_number _last_sn {0};

    std::vector<bool> _parts_received;
    std::vector<char> _payload;
    std::size_t _remain_parts {0};

public:
    /**
     *  Constructs multipart message assembler.
     */
    multipart_assembler (std::string && msgid, std::uint64_t total_size, std::uint32_t part_size
        , serial_number first_sn, serial_number last_sn)
        : _msgid(std::move(msgid))
        , _part_size(part_size)
        , _first_sn(first_sn)
        , _last_sn(last_sn)
    {
        if (_last_sn < _first_sn) {
            throw error {
                  make_error_code(std::errc::invalid_argument)
                , "bad serial number bounds"
            };
        }

        _remain_parts = _last_sn - _first_sn + 1;
        _payload.resize(total_size);
        _parts_received.resize(_remain_parts, false);
    }

public:
    std::string const & msgid () const noexcept
    {
        return _msgid;
    }

    void emplace_part (serial_number sn, std::vector<char> && part, bool replace = false)
    {
        if (!(sn >= _first_sn && sn <= _last_sn)) {
            throw error {
                  make_error_code(std::errc::invalid_argument)
                , "serial number is out of bounds"
            };
        }

        if (part.size() > _part_size) {
            throw error {
                  make_error_code(std::errc::invalid_argument)
                , "part size too big"
            };
        }

        std::size_t index = sn - _first_sn;

        // Already received
        if (_parts_received[index]) {
            if (!replace)
                return;
        } else {
            PFS__THROW_UNEXPECTED(_remain_parts > 0, "Fix delivery::multipart_assembler algorithm");
            _remain_parts--;
        }

        std::copy(part.begin(), part.end(), _payload.begin() + (index * _part_size));
        _parts_received[index] = true;
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

    std::vector<char> payload () noexcept
    {
        auto result = std::move(_payload);
        return result;
    }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
