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
#include "node_index.hpp"
#include <chrono>
#include <string>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdTraits>
class node_interface
{
public:
    using node_id = typename NodeIdTraits::type;
    using node_id_rep = typename NodeIdTraits::rep_type;

public:
    virtual ~node_interface () {}

public:
    virtual node_id id () const noexcept = 0;
    virtual node_id_rep id_rep () const noexcept = 0;
    virtual std::string name () const noexcept = 0;

    virtual void set_index (node_index_t index) noexcept = 0;
    virtual node_index_t index () const noexcept = 0;

    virtual void add_listener (netty::socket4_addr const & listener_addr, error * perr = nullptr) = 0;
    virtual bool connect_host (netty::socket4_addr remote_saddr, bool behind_nat = false) = 0;
    virtual bool connect_host (netty::socket4_addr remote_saddr, netty::inet4_addr local_addr, bool behind_nat) = 0;
    virtual void listen (int backlog = 50) = 0;
    virtual void enqueue (node_id_rep const & id, int priority, bool force_checksum, char const * data, std::size_t len) = 0;
    virtual void enqueue (node_id_rep const & id, int priority, bool force_checksum, std::vector<char> && data) = 0;
    virtual bool has_writer (node_id_rep const & id) const = 0;
    virtual unsigned int step () = 0;
    virtual void clear_channels () = 0;

    // For internal use only
    virtual void enqueue_packet (node_id_rep const & id, int priority, std::vector<char> && data) = 0;
    virtual void enqueue_packet (node_id_rep const & id, int priority, char const * data, std::size_t len) = 0;
    virtual void enqueue_broadcast_packet (int priority, char const * data, std::size_t len) = 0;
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
