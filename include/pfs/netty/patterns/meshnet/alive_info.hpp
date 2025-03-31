////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "node_id_rep.hpp"
#include <cstdint>
#include <utility>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

struct alive_info
{
    node_id_rep id;
};

struct unreachable_info
{
    node_id_rep id;
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
