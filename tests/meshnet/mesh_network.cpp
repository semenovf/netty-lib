////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.12.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "mesh_network.hpp"
#include "pfs/netty/socket4_addr.hpp"

mesh_network * mesh_network::_self {nullptr};

static void sigterm_handler (int sig)
{
    LOGD(TAG, "Force interrupt all nodes by signal: ", sig);
    mesh_network::instance()->interrupt_all();
}

mesh_network::mesh_network (std::initializer_list<std::string> node_names)
    : _sigint_guard(SIGINT, sigterm_handler)
{
    PFS__ASSERT(_self == nullptr, "");
    _self = this;

    std::size_t counter = 0;

    for (auto const & name: node_names) {
        auto pctx = std::make_shared<context>();
        pctx->name = name;
        pctx->node_pool_ptr = create_node_pool(name);
        pctx->index = counter++;
        auto res = _node_pools.insert({name, pctx});
        PFS__ASSERT(res.second, "");
        (void)res;
    }
}

mesh_network::~mesh_network ()
{
    PFS__ASSERT(_self != nullptr, "");
    join();
    _self = nullptr;
}

std::unique_ptr<node_pool_t> mesh_network::create_node_pool (std::string const & name)
{
    auto entry = _dict.get_entry(name);
    netty::socket4_addr listener_saddr {netty::inet4_addr {127, 0, 0, 1}, entry.port};

    auto ptr = std::make_unique<node_pool_t>(entry.id, entry.is_gateway);

    ptr->on_error([] (std::string const & errstr)
    {
        LOGE(TAG, "{}", errstr);
    });

    ptr->on_channel_established([this, name] (meshnet_ns::node_index_t index, node_id peer_id
        , bool is_gateway)
    {
        this->on_channel_established(name, index, _dict.get_entry(peer_id).name, is_gateway);
    });

    ptr->on_channel_destroyed([this, name] (node_id peer_id)
    {
        this->on_channel_destroyed(name, _dict.get_entry(peer_id).name);
    });

    ptr->on_duplicate_id([this, name] (node_id peer_id, netty::socket4_addr saddr)
    {
        this->on_duplicate_id(name, _dict.get_entry(peer_id).name, saddr);
    });

    ptr->on_node_alive([this, name] (node_id peer_id)
    {
        this->on_node_alive(name, _dict.get_entry(peer_id).name);
    });

    ptr->on_node_expired([this, name] (node_id peer_id)
    {
        this->on_node_expired(name, _dict.get_entry(peer_id).name);
    });

    ptr->on_route_ready([this, name] (node_id peer_id, std::vector<node_id> gw_chain)
    {
        auto peer_name = _dict.get_entry(peer_id).name;
        this->on_route_ready(name, peer_name, std::move(gw_chain)
            , get_context_ptr(name)->index
            , get_context_ptr(peer_name)->index);
    });

    ptr->template add_node<node_t>({listener_saddr});

    return ptr;
}

void mesh_network::listen_all ()
{
    for (auto & x: _node_pools)
        x.second->node_pool_ptr->listen();
}

void mesh_network::connect (std::string const & initiator_name, std::string const & peer_name
    , bool behind_nat)
{
    netty::meshnet::node_index_t index = 1;
    auto initiator_ctx = get_context_ptr(initiator_name);
    auto const & peer_entry = _dict.get_entry(peer_name);

    netty::socket4_addr peer_saddr {netty::inet4_addr {127, 0, 0, 1}, peer_entry.port};
    initiator_ctx->node_pool_ptr->connect_host(index, peer_saddr, behind_nat);
}

void mesh_network::disconnect (std::string const & initiator_name, std::string const & peer_name)
{
    netty::meshnet::node_index_t index = 1;
    auto initiator_ctx = get_context_ptr(initiator_name);
    auto peer_ctx = get_context_ptr(peer_name);

    initiator_ctx->node_pool_ptr->disconnect(index, peer_ctx->node_pool_ptr->id());
}

void mesh_network::run_all ()
{
    for (auto & x: _node_pools) {
        auto pctx = x.second;

        std::thread th {
            [this, pctx] () {
                LOGD(TAG, "{}: thread started", pctx->name);
                pctx->node_pool_ptr->run();
                LOGD(TAG, "{}: thread finished", pctx->name);
            }
        };

        pctx->node_thread = std::move(th);
    }

    _scenario_thread = std::thread {_scenario};

    join();
}

void mesh_network::interrupt_all ()
{
    for (auto & x: _node_pools)
        x.second->node_pool_ptr->interrupt();
}

void mesh_network::join ()
{
    for (auto & x: _node_pools) {
        if (x.second->node_thread.joinable())
            x.second->node_thread.join();
    }

    if (_scenario_thread.joinable())
        _scenario_thread.join();
}
