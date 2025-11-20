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
#include "../../buffer.hpp"
#include "envelope.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <pfs/optional.hpp>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace pubsub {

template <typename Archive>
class input_account
{
    using envelope_type = envelope<Archive>;
    using buffer_type   = buffer<Archive>;

private:
    buffer_type _raw; // Buffer to accumulate chunks

public:
    void append_chunk (Archive chunk)
    {
        _raw.append(std::move(chunk));
    }

    auto next ()
    {
        typename envelope_type::parser parser{_raw.data(), _raw.size()};
        auto opt = parser.next();

        if (opt) {
            std::size_t parsed_size = _raw.size() - parser.remain_size();
            _raw.erase_front(parsed_size);
        } else {
            PFS__THROW_UNEXPECTED(!parser.bad(), tr::_("bad or corrupted envelope received"));
        }

        return opt;
    }
};

}} // namespace patterns::pubsub

NETTY__NAMESPACE_END
