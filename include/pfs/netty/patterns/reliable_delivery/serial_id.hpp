////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.14 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/namespace.hpp>
#include <cstdint>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace reliable_delivery {

using serial_id = std::uint64_t;

}} // namespace patterns::reliable_delivery

NETTY__NAMESPACE_END


