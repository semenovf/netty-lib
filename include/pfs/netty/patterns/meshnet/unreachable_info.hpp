////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.13 Initial version.
//      2025.05.12 `node_id_rep` replaced by `std::string`.
//      2025.12.14 Removed `alive_info` and header file renamed to `unreachable_info`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <string>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

template <typename NodeId>
struct unreachable_info
{
    NodeId gw_id; // gateway ID that fixed channel disconnection
    NodeId id;    // unreachable node ID
};

} // namespace meshnet

NETTY__NAMESPACE_END
