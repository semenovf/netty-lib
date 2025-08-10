////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "envelope.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <pfs/optional.hpp>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace pubsub {

class input_account
{
    using envelope_type = envelope_t;

private:
    std::vector<char> _raw; // Buffer to accumulate chunks

public:
    void append_chunk (std::vector<char> && chunk)
    {
        _raw.insert(_raw.end(), chunk.begin(), chunk.end());
    }

    auto next ()
    {
        envelope_type::parser parser{_raw.data(), _raw.size()};
        auto opt = parser.next();

        if (opt) {
            std::size_t parsed_size = _raw.size() - parser.remain_size();
            _raw.erase(_raw.begin(), _raw.begin() + parsed_size);
        } else {
            PFS__THROW_UNEXPECTED(!parser.bad(), tr::_("bad or corrupted envelope received"));
        }

        return opt;
    }
};

}} // namespace patterns::pubsub

NETTY__NAMESPACE_END
