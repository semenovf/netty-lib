////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/namespace.hpp>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class null_input_processor
{
public:
    null_input_processor () {}

public:
    void configure () {}
    void remove (typename Node::socket_id) {}
    void process_input (typename Node::socket_id, std::vector<char> &&) {}
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
