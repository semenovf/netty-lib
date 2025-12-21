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
    , _node_names(std::move(node_names))
{
    PFS__ASSERT(_self == nullptr, "");
    _self = this;

    std::size_t counter = 0;

    for (auto const & name: _node_names) {
        auto pctx = std::make_shared<context>();
        pctx->name = name;
        pctx->node_ptr = create_node(name);
        pctx->index = counter++;
        auto res = _nodes.insert({name, pctx});
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

std::unique_ptr<node_t> mesh_network::create_node (std::string const & name)
{
    auto entry = _dict.get_entry(name);
    netty::socket4_addr listener_saddr {netty::inet4_addr {127, 0, 0, 1}, entry.port};

    auto ptr = std::make_unique<node_t>(entry.id, entry.is_gateway);

    ptr->on_error([name] (std::string const & errstr)
    {
        LOGE(TAG, "{}: {}", name, errstr);
    });

    ptr->on_channel_established([this, name] (meshnet_ns::peer_index_t index, node_id peer_id
        , bool is_gateway)
    {
        this->on_channel_established(make_spec(name), index, make_spec(peer_id), is_gateway);
    });

    ptr->on_channel_destroyed([this, name] (node_id peer_id)
    {
        this->on_channel_destroyed(make_spec(name), make_spec(peer_id));
    });

    ptr->on_duplicate_id([this, name] (node_id peer_id, netty::socket4_addr saddr)
    {
        this->on_duplicate_id(make_spec(name), make_spec(peer_id), saddr);
    });

    ptr->on_route_ready([this, name] (node_id dest_id, std::size_t route_index)
    {
        this->on_route_ready(make_spec(name), make_spec(dest_id), route_index);
    });

    ptr->on_route_lost([this, name] (node_id dest_id, std::size_t route_index)
    {
        this->on_route_lost(make_spec(name), make_spec(dest_id), route_index);
    });

    ptr->on_node_unreachable([this, name] (node_id peer_id)
    {
        this->on_node_unreachable(make_spec(name), make_spec(peer_id));
    });

#ifdef NETTY__TESTS_USE_MESHNET_NODE_POOL_RD
    // TODO
#else
    ptr->on_data_received([this, name] (node_id sender_id, int priority, archive_t bytes)
    {
        this->on_data_received(make_spec(name), make_spec(sender_id), priority, std::move(bytes));
    });
#endif

    ptr->template add<peer_t>({listener_saddr});

    return ptr;
}

void mesh_network::listen_all ()
{
    for (auto & x : _nodes) {
        if (x.second->node_ptr)
            x.second->node_ptr->listen();
    }
}

void mesh_network::connect (std::string const & initiator_name, std::string const & peer_name
    , bool behind_nat)
{
    netty::meshnet::peer_index_t index = 1;
    auto initiator_ctx = get_context_ptr(initiator_name);
    auto const & peer_entry = _dict.get_entry(peer_name);

    netty::socket4_addr peer_saddr {netty::inet4_addr {127, 0, 0, 1}, peer_entry.port};
    initiator_ctx->node_ptr->connect_peer(index, peer_saddr, behind_nat);
}

void mesh_network::disconnect (std::string const & initiator_name, std::string const & peer_name)
{
    netty::meshnet::peer_index_t index = 1;
    auto initiator_ctx = get_context_ptr(initiator_name);
    auto peer_ctx = get_context_ptr(peer_name);

    PFS__ASSERT(initiator_ctx->node_ptr, "Fix disconnect() method call");
    initiator_ctx->node_ptr->disconnect(index, peer_ctx->node_ptr->id());
}

void mesh_network::destroy (std::string const & name)
{
    auto pctx = get_context_ptr(name);

    PFS__ASSERT(pctx->node_ptr, "Fix destroy() method call");

    pctx->node_ptr->interrupt();

    if (pctx->node_thread.joinable())
        pctx->node_thread.join();

    // Destroy node pool
    pctx->node_ptr.reset();
}

void mesh_network::send_message (std::string const & sender_name, std::string const & receiver_name
    , std::string const & text, int priority)
{
    auto sender_ctx = get_context_ptr(sender_name);
    // auto receiver_ctx = get_context_ptr(receiver_name);
    auto receiver_id = _dict.get_entry(receiver_name).id;

    PFS__ASSERT(sender_ctx->node_ptr, "Fix send_message() method call");

#ifdef NETTY__TESTS_USE_MESHNET_NODE_POOL_RD
    // TODO
//     message_id msgid = pfs::generate_uuid();
//
//     sender_ctx->node_ptr->enqueue_message(receiver_id, msgid, priority, text.data()
//         , text.size());
#else
    sender_ctx->node_ptr->enqueue(receiver_id, priority, text.data(), text.size());
#endif
}

void mesh_network::run_all ()
{
    for (auto & x: _nodes) {
        auto pctx = x.second;

        std::thread th {
            [this, pctx] () {
                if (pctx->node_ptr) {
                    LOGD(TAG, "{}: thread started", pctx->name);
                    pctx->node_ptr->run();
                    LOGD(TAG, "{}: thread finished", pctx->name);
                }
            }
        };

        pctx->node_thread = std::move(th);
    }

    _scenario_thread = std::thread {_scenario};

    join();
}

void mesh_network::interrupt_all ()
{
    for (auto & x : _nodes) {
        if (x.second->node_ptr)
            x.second->node_ptr->interrupt();
    }
}

void mesh_network::join ()
{
    for (auto & x: _nodes) {
        if (x.second->node_thread.joinable())
            x.second->node_thread.join();
    }

    if (_scenario_thread.joinable())
        _scenario_thread.join();
}

void mesh_network::print_routing_records (std::string const & name)
{
    auto pctx = get_context_ptr(name);

    if (pctx->node_ptr) {
        auto routes = pctx->node_ptr->dump_routing_table();

        LOGD(TAG, "┌────────────────────────────────────────────────────────────────────────────────");
        LOGD(TAG, "│Routes for: {}", name);

        for (auto const & x : routes) {
            LOGD(TAG, "│    └──── {}", x);
        }

        LOGD(TAG, "└────────────────────────────────────────────────────────────────────────────────");
    }
}
