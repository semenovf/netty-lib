////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.25 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

enum class handshake_result_enum
{
      success = 0    // Handshake completed successfully
    , rejected = 1   // Channel already estabslished
    , duplicated = 2 // Attempt to establish a connection with a node that has the same identifier
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

