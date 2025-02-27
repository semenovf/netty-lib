////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.25 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/namespace.hpp>
#include <pfs/netty/socket4_addr.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename Node>
class without_handshake
{
    using socket_id = typename Node::socket_id;
    using serializer_traits = typename Node::serializer_traits;

public:
    without_handshake (Node &) {}

public:
    template <typename F> without_handshake & on_expired (F &&) { return *this; }
    template <typename F> without_handshake & on_failure (F &&) { return *this; }
    template <typename F> without_handshake & on_completed (F &&)  { return *this; }

    void start (socket_id) {}
    void cancel (socket_id) {}
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END



