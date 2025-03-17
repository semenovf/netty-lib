////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class without_message_sender
{
    using socket_id = typename Node::socket_id;

public:
    without_message_sender (Node &)
    {}

public:
    void send (socket_id, int, bool, char const *, std::size_t)
    {}

    void send (socket_id, int, bool, std::vector<char> &&)
    {}
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
