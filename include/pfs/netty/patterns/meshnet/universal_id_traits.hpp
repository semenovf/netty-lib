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
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
