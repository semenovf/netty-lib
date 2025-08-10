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
#include "input_account.hpp"
#include "tag.hpp"
#include <unordered_map>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace pubsub {

class input_controller
{
    using account_type = input_account;

protected:
    account_type _acc;

public:
    input_controller () = default;

public: // Callbacks
    mutable callback_t<void (std::vector<char> &&)> on_data_ready = [] (std::vector<char> &&) {};

public:
    void process_input (std::vector<char> && chunk)
    {
        if (chunk.empty())
            return;

        _acc.append_chunk(std::move(chunk));

        while (auto opt_msg = _acc.next())
            on_data_ready(std::move(*opt_msg));
    }
};

}} // namespace patterns::pubsub

NETTY__NAMESPACE_END
