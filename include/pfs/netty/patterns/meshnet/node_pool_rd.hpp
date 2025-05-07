    ////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.05.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../callback.hpp"

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

/**
 * Node pool with reliable delivery support
 */
template <typename NodePool, typename DeliveryManager>
class node_pool_rd: protected NodePool
{
    using base_class = NodePool;
    using delivery_manager_type = DeliveryManager;

public:
    struct options: typename base_class::options
    {};

private:
    std::unique_ptr<delivery_manager_type> _dm;

public:
    node_pool_rd (options && opts)
        : base_class(opts)
    {}

    node_pool_rd (node_pool_rd const &) = delete;
    node_pool_rd (node_pool_rd &&) = delete;
    node_pool_rd & operator = (node_pool_rd const &) = delete;
    node_pool_rd & operator = (node_pool_rd &&) = delete;

    ~node_pool_rd () = default;

private:
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END

