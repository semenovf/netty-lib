////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.12.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "node_dictionary.hpp"
#include "pfs/netty/startup.hpp"
#include <pfs/assert.hpp>
#include <pfs/log.hpp>
#include <pfs/signal_guard.hpp>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>

static constexpr char const * TAG = "test::meshnet";

// Node pool name + Node Pool index in the meshnet node list
using node_pool_spec_t = std::pair<std::string, std::size_t>;

class mesh_network
{
private:
    struct context
    {
        std::string name;
        std::unique_ptr<node_pool_t> node_pool_ptr;
        std::thread node_thread;
        std::size_t index {0}; // Index used in matrix to check tests results
    };

private:
    node_dictionary _dict;
    std::map<std::string, std::shared_ptr<context>> _node_pools;
    std::thread _scenario_thread;
    std::function<void ()> _scenario;

    pfs::signal_guard _sigint_guard;
    netty::startup_guard _startup_guard;

private:
    static mesh_network * _self;

public:
    netty::callback_t<void (node_pool_spec_t const &, netty::meshnet::node_index_t
        , node_pool_spec_t const &, bool)>
    on_channel_established = [] (node_pool_spec_t /*source*/, netty::meshnet::node_index_t
        , node_pool_spec_t /*peer_name*/, bool /*is_gateway*/)
    {};

    netty::callback_t<void (node_pool_spec_t const &, node_pool_spec_t const &)>
    on_channel_destroyed = [] (node_pool_spec_t const &/*source*/, node_pool_spec_t const & /*peer*/)
    {};

    netty::callback_t<void (node_pool_spec_t const &, node_pool_spec_t const &
        , netty::socket4_addr saddr)>
    on_duplicate_id = [] (node_pool_spec_t const & /*source*/, node_pool_spec_t const & /*peer*/
        , netty::socket4_addr saddr)
    {};

    netty::callback_t<void (node_pool_spec_t const &, node_pool_spec_t const &)>
    on_node_alive = [] (node_pool_spec_t const & /*source*/, node_pool_spec_t const & /*dest*/)
    {};

    netty::callback_t<void (node_pool_spec_t const &, node_pool_spec_t const &)>
    on_node_unreachable = [] (node_pool_spec_t const & /*source*/, node_pool_spec_t const & /*dest*/)
    {};

    netty::callback_t<void (node_pool_spec_t const &, node_pool_spec_t const &
        , std::vector<node_id> const &)>
    on_route_ready = [] (node_pool_spec_t const & /*source*/, node_pool_spec_t const & /*peer*/
        , std::vector<node_id> const & /*gw_chain*/)
    {};

#ifdef NETTY__TESTS_USE_MESHNET_NODE_POOL_RD
    // TODO
#else
    netty::callback_t<void (node_pool_spec_t const &, node_pool_spec_t const &, int, archive_t)>
    on_data_received = [] (node_pool_spec_t const & /*receiver*/, node_pool_spec_t const & /*sender*/
        , int /*priority*/, archive_t /*bytes*/)
    {};
#endif

public:
    mesh_network (std::initializer_list<std::string> node_names);
    ~mesh_network ();

public:
    template <typename Scenario>
    void set_scenario (Scenario && sc)
    {
        _scenario = std::forward<Scenario>(sc);
    }

    void listen_all ();
    void connect (std::string const & initiator_name, std::string const & peer_name
        , bool behind_nat = false);
    void disconnect (std::string const & initiator_name, std::string const & peer_name);
    void destroy (std::string const & name);

    void send_message (std::string const & sender_name, std::string const & receiver_name
        , std::string const & text, int priority = 1);

    void run_all ();
    void interrupt_all ();
    void print_routing_records (std::string const & name);

private:
    std::shared_ptr<context> get_context_ptr (std::string const & name) const
    {
        return _node_pools.at(name);
    }

    std::unique_ptr<node_pool_t> create_node_pool (std::string const & name);

    node_pool_spec_t make_spec (std::string const & name) const
    {
        return std::make_pair(name, get_context_ptr(name)->index);
    }

    node_pool_spec_t make_spec (node_id id) const
    {
        return make_spec(_dict.get_entry(id).name);
    }

    void join ();

public: // static
    static mesh_network * instance ()
    {
        PFS__ASSERT(_self != nullptr, "");
        return _self;
    }
};
