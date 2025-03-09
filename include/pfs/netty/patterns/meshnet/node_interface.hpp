////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.25 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../error.hpp"
#include "../../namespace.hpp"
#include "../../socket4_addr.hpp"
#include <chrono>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdTraits>
class node_interface
{
public:
    using node_id = typename NodeIdTraits::node_id;

public:
    virtual ~node_interface () {}

public:
    virtual void add_listener (netty::socket4_addr const & listener_addr, error * perr = nullptr) = 0;
    virtual bool connect_host (netty::socket4_addr remote_saddr) = 0;
    virtual bool connect_host (netty::socket4_addr remote_saddr, netty::inet4_addr local_addr) = 0;
    virtual void listen (int backlog = 50) = 0;
    virtual void enqueue (node_id id, int priority, bool force_checksum, char const * data, std::size_t len) = 0;
    virtual void enqueue (node_id id, int priority, bool force_checksum, std::vector<char> && data) = 0;
    virtual bool has_writer (node_id id) const = 0;
    virtual void step (std::chrono::milliseconds millis = std::chrono::milliseconds{0}) = 0;

    // For internal use only
    virtual void enqueue_packet (node_id id, int priority, std::vector<char> && data) = 0;
    virtual void enqueue_packet (node_id id, int priority, char const * data, std::size_t len) = 0;
    virtual void enqueue_broadcast_packet (int priority, char const * data, std::size_t len) = 0;
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
