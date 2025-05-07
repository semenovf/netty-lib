////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.04 Initial version.
//      2025.05.07 Renamed from without_input_processor.hpp to without_input_controller.hpp.
//                 Class `without_input_processor` renamed to `without_input_controller`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class without_input_controller
{
    using socket_id = typename Node::socket_id;

public:
    without_input_controller (Node *) {}

public:
    void add (socket_id) {}
    void remove (socket_id) {}
    void process_input (socket_id, std::vector<char> &&) {}
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
