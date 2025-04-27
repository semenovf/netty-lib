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
#include <functional>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

template <typename AddressType>
struct delivery_callbacks
{
    // std::function<void (std::string const &)> on_error
    //     = [] (std::string const & msg) { LOGE("[node]", "{}", msg); };

    std::function<void (AddressType)> on_receiver_ready = [] (AddressType) {};
};

}} // namespace patterns::delivery

NETTY__NAMESPACE_END

