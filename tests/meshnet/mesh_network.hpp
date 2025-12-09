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
#include <functional>
#include <map>
#include <memory>
// #include <stdexcept>
#include <string>
#include <thread>
// #include <vector>

static constexpr char const * TAG = "test::meshnet";

class mesh_network
{
private:
    struct context
    {
        std::string name;
        std::unique_ptr<node_pool_t> node_pool_ptr;
        std::thread node_thread;
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
    netty::callback_t<void (std::string const &, netty::meshnet::node_index_t, std::string const &, bool)>
    on_channel_established = [] (std::string const & /*source_name*/
        , netty::meshnet::node_index_t
        , std::string const & /*peer_name*/
        , bool /*is_gateway*/)
    {};

    netty::callback_t<void (std::string const &, std::string const &)>
    on_channel_destroyed = [] (std::string const & /*source_name*/
        , std::string const & /*peer_name*/)
    {};

    netty::callback_t<void (std::string const &, std::string const &, netty::socket4_addr saddr)>
    on_duplicate_id = [] (std::string const & /*source_name*/
        , std::string const & /*peer_name*/
        , netty::socket4_addr saddr)
    {};

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
    void run_all ();
    void interrupt_all ();

private:
    std::shared_ptr<context> & get_context_ptr (std::string const & name)
    {
        return _node_pools.at(name);
    }

    std::unique_ptr<node_pool_t> create_node_pool (std::string const & name);

public: // static
    static mesh_network * instance ()
    {
        PFS__ASSERT(_self != nullptr, "");
        return _self;
    }
};
