////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/namespace.hpp>
#include <functional>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

struct functional_callbacks
{
    std::function<void()> on_connected;
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

