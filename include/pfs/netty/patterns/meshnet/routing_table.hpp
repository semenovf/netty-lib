////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
// #include "handshake_result.hpp"
// #include "unordered_bimap.hpp"
// #include "../../conn_status.hpp"
// #include "../../connection_refused_reason.hpp"
// #include "../../error.hpp"
#include "../../namespace.hpp"
// #include "../../socket4_addr.hpp"
// #include "../../connecting_pool.hpp"
// #include "../../listener_pool.hpp"
// #include "../../reader_pool.hpp"
// #include "../../socket_pool.hpp"
// #include "../../writer_pool.hpp"
// #include <pfs/countdown_timer.hpp>
// #include <pfs/i18n.hpp>
// #include <vector>
// #include <unordered_map>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdTraits>
class routing_table
{
public:
    using node_id = typename NodeIdTraits::node_id;

private:

public:
    routing_table () {}

    routing_table (routing_table const &) = delete;
    routing_table (routing_table &&) = delete;
    routing_table & operator = (routing_table const &) = delete;
    routing_table & operator = (routing_table &&) = delete;

    ~routing_table () = default;

public:
    /**
     * Processes routing data.
     *
     * @param id Node
     */
    void process (bool is_response, std::vector<std::pair<std::uint64_t, std::uint64_t>> && route)
    {
    };
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
