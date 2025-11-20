////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.13 Initial version.
//      2025.05.12 `node_id_rep` replaced by `std::string`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <string>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

template <typename NodeId>
struct alive_info
{
    NodeId id;
};

template <typename NodeId>
struct unreachable_info
{
    NodeId gw_id;       // Last gateway ID
    NodeId sender_id;   // Sender node ID
    NodeId receiver_id; // Unreachable node ID
};

} // namespace meshnet

NETTY__NAMESPACE_END
