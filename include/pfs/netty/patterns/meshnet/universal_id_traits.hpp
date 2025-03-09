////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.25 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/optional.hpp>
#include <pfs/universal_id.hpp>
#include <pfs/universal_id_hash.hpp>
#include <string>
#include <utility>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

struct universal_id_traits
{
    using node_id = pfs::universal_id;

    static std::string stringify (node_id const & id)
    {
        return to_string(id);
    }

    static pfs::optional<node_id> parse (char const * s, std::size_t n)
    {
        return pfs::parse_universal_id(s, n);
    }

    /**
     * Low 64-bit part of the node ID binary representation
     */
    static std::uint64_t low (node_id const & id)
    {
        return PFS__NAMESPACE_NAME::low(id);
    }

    /**
     * High 64-bit part of the node ID binary representation
     */
    static std::uint64_t high (node_id const & id)
    {
        return PFS__NAMESPACE_NAME::high(id);
    }

    static node_id make (std::uint64_t high, std::uint64_t low)
    {
        return pfs::make_uuid(high, low);
    }

    static node_id make (std::pair<std::uint64_t, std::uint64_t> const & id_pair)
    {
        return pfs::make_uuid(id_pair.first, id_pair.second);
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
