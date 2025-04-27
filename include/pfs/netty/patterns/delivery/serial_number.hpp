////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.14 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <cstdint>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace delivery {

// Serial number starts from 1 (0 - invalid serial number, value for initialization)
using serial_number = std::uint64_t;

}} // namespace patterns::delivery

NETTY__NAMESPACE_END
