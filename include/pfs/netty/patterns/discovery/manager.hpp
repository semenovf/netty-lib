////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
// #include "protocol.hpp"
// #include "serial_id.hpp"
// #include "../../error.hpp"
// #include <pfs/assert.hpp>
// #include <pfs/i18n.hpp>
// #include <pfs/utility.hpp>
// #include <chrono>
// #include <functional>
// #include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace discovery {

/*
 * Callbacks API:
 *--------------------------------------------------------------------------------------------------
 * class callbacks
 * {
 * public:
 * }
 */
template <typename NodeIdTraits>
class manager
{
    using node_id_traits = NodeIdTraits;
    using node_id = typename NodeIdTraits::node_id;

private:
    node_id _id;
public:
    manager (node_id id)
        : _id(id)
    {}

    manager (manager const &) = delete;
    manager (manager &&) = delete;
    manager & operator = (manager const &) = delete;
    manager & operator = (manager &&) = delete;

    ~manager () {}

public:
};

}} // namespace patterns::discovery

NETTY__NAMESPACE_END
