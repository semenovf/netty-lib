////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "peer_id.hpp"
#include <functional>
#include <vector>

namespace netty {
namespace p2p {

struct functional_reliable_delivery_callbacks
{
    /**
     * Message received.
     */
    mutable std::function<void (peer_id, std::vector<char>)> message_received
        = [] (peer_id, std::vector<char>) {};
};

}} // namespace netty::p2p
