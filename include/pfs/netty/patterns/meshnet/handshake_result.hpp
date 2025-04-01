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
      success = 0     // Handshake completed successfully
    , reject = 1      // Used by single link handshakes to signal that socket can be closed
    , duplicated = 2  // Attempt to establish a connection with a node that has the same identifier
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

