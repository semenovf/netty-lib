////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <pfs/log.hpp>
#include <functional>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

template <typename AddressType, typename MessageId>
struct delivery_callbacks
{
    std::function<void (std::string const &)> on_error
        = [] (std::string const & msg) { LOGE("[delivery]", "{}", msg); };

    std::function<void (AddressType)> on_receiver_ready = [] (AddressType) {};
    std::function<void (AddressType, std::vector<char> &&)> on_message_received
        = [] (AddressType, std::vector<char> &&) {};

    std::function<void (AddressType, MessageId)> on_message_dispatched
        = [] (AddressType, MessageId) {};

    std::function<void (AddressType, std::vector<char> &&)> on_report_received
        = [] (AddressType, std::vector<char> &&) {};
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
