////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "protocol.hpp"
#include "serial_number.hpp"
#include <algorithm>
#include <cstdint>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

class multipart_assembler
{
    std::uint32_t _part_size {0};
    serial_number _initial_sn {0};
    serial_number _last_sn {0};

    std::vector<bool> _parts_received;
    std::vector<char> _payload;

public:
    /**
     *  Constructs multipart message assembler.
     */
    multipart_assembler (std::uint64_t total_size, std::uint32_t part_size
        , serial_number initial_sn, serial_number last_sn)
        : _part_size(part_size)
        , _initial_sn(initial_sn)
        , _last_sn(last_sn)
    {
        _payload.resize(total_size);
        _parts_received.resize(total_size, false);
    }

public:
    void emplace_part (serial_number sn, std::vector<char> && part, bool replace = false)
    {
        PFS__TERMINATE(sn >= _initial_sn && sn <= _last_sn, "Unexpected error");
        PFS__TERMINATE(part.size() <= _part_size, "Unexpected error");

        std::size_t index = sn - _initial_sn;

        // Already received
        if (_parts_received[index] && !replace)
            return;

        // TODO Check out of bounds

        std::copy(part.begin(), part.end(), _payload.begin() + (index * _part_size));
        _parts_received[index] = true;
    }
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
