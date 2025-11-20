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
#include "../../callback.hpp"
#include "../../traits/archive_traits.hpp"
#include "input_account.hpp"
#include "tag.hpp"
#include <unordered_map>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace pubsub {

template <typename Archive>
class input_controller
{
    using account_type = input_account<Archive>;
    using archive_traits_type = archive_traits<Archive>;

public:
    using archive_type = typename archive_traits_type::archive_type;

protected:
    account_type _acc;

public:
    input_controller () = default;

public: // Callbacks
    mutable callback_t<void (archive_type)> on_data_ready = [] (archive_type) {};

public:
    void process_input (archive_type chunk)
    {
        if (archive_traits_type::empty(chunk))
            return;

        _acc.append_chunk(std::move(chunk));

        while (auto opt_msg = _acc.next())
            on_data_ready(std::move(*opt_msg));
    }
};

}} // namespace patterns::pubsub

NETTY__NAMESPACE_END
