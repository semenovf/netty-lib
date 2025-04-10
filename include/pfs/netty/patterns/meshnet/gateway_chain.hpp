////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "node_id_rep.hpp"
#include <string>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

using gateway_chain_t = std::vector<node_id_rep>;

inline gateway_chain_t reverse_gateway_chain (gateway_chain_t const & gw_chain)
{
    gateway_chain_t reversed_gw_chain(gw_chain.size());
    std::reverse_copy(gw_chain.begin(), gw_chain.end(), reversed_gw_chain.begin());
    return reversed_gw_chain;
}

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
