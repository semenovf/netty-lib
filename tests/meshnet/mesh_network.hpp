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
#include <pfs/lorem/wait_bitmatrix.hpp>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

static constexpr char const * TAG = "test::meshnet";

// Node ame + Node index in the meshnet node list
using node_spec_t = std::pair<std::string, std::size_t>;

class mesh_network
{
private:
    struct context
    {
        std::string name;
        std::unique_ptr<node_t> node_ptr;
        std::thread node_thread;
        std::size_t index {0}; // Index used in matrix to check tests results
    };

private:
    node_dictionary _dict;
    std::vector<std::string> _node_names;
    std::map<std::string, std::shared_ptr<context>> _nodes;
    std::thread _scenario_thread;
    std::function<void ()> _scenario;

    pfs::signal_guard _sigint_guard;
    netty::startup_guard _startup_guard;

    std::atomic_bool _is_running {false};

private:
    static mesh_network * _self;

public:
    netty::callback_t<void (node_spec_t const &, netty::meshnet::peer_index_t
        , node_spec_t const &, bool)>
    on_channel_established = [] (node_spec_t /*source*/, netty::meshnet::peer_index_t
        , node_spec_t /*peer_name*/, bool /*is_gateway*/)
    {};

    netty::callback_t<void (node_spec_t const &, node_spec_t const &)>
    on_channel_destroyed = [] (node_spec_t const &/*source*/, node_spec_t const & /*peer*/)
    {};

    netty::callback_t<void (node_spec_t const &, node_spec_t const &
        , netty::socket4_addr saddr)>
    on_duplicate_id = [] (node_spec_t const & /*source*/, node_spec_t const & /*peer*/
        , netty::socket4_addr saddr)
    {};

    netty::callback_t<void (node_spec_t const &, node_spec_t const &, std::size_t)>
    on_route_ready = [] (node_spec_t const & /*source*/, node_spec_t const & /*dest*/
        , std::size_t /*route_index*/)
    {};

    netty::callback_t<void (node_spec_t const &, node_spec_t const &, std::size_t)>
    on_route_lost = [] (node_spec_t const & /*source*/, node_spec_t const & /*dest*/
        , std::size_t /*route_index*/)
    {};

    netty::callback_t<void (node_spec_t const & /*source*/, node_spec_t const & /*dest*/)>
    on_node_unreachable = [] (node_spec_t const &, node_spec_t const &) {};

#ifdef NETTY__TESTS_USE_MESHNET_RELIABLE_NODE
    // TODO

    netty::callback_t<void (node_spec_t const & /*receiver*/, node_spec_t const & /*sender*/
        , std::string const & /*msgid*/, int /*priority*/, archive_t /*msg*/)>
    on_message_received = [] (node_spec_t const & , node_spec_t const &, std::string const &, int, archive_t)
    {};
#else
    netty::callback_t<void (node_spec_t const & /*receiver*/, node_spec_t const & /*sender*/
        , int /*priority*/, archive_t /*bytes*/)>
    on_data_received = [] (node_spec_t const & , node_spec_t const & , int , archive_t) {};
#endif

public:
    mesh_network (std::initializer_list<std::string> node_names);
    ~mesh_network ();

public:
    std::vector<std::string> const & node_names () const noexcept
    {
        return _node_names;
    }

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
        , std::string const & bytes, int priority = 1);

    void run_all ();
    void interrupt_all ();
    void print_routing_records (std::string const & name);

    bool is_running () const
    {
        return _is_running;
    }

    template <std::size_t N>
    inline void set (lorem::wait_bitmatrix<N> & m, std::string const & source_name
        , std::string const & target_name, bool value = true)
    {
        auto source_ctx = get_context_ptr(source_name);
        auto target_ctx = get_context_ptr(target_name);

        m.set(source_ctx->index, target_ctx->index, value);
    }

    template <std::size_t N>
    inline void set_row (lorem::wait_bitmatrix<N> & m, std::string const & name, bool value = true)
    {
        auto pctx = get_context_ptr(name);

        for (std::size_t i = 0; i < N; i++)
            m.set(pctx->index, i, value);
    }

    template <std::size_t N>
    static void set_main_diagonal (lorem::wait_bitmatrix<N> & matrix, bool value = true)
    {
        for (std::size_t i = 0; i < N; i++)
            matrix.set(i, i, value);
    }

    template <std::size_t N>
    static void set_all (lorem::wait_bitmatrix<N> & matrix, bool value = true)
    {
        for (std::size_t i = 0; i < N; i++)
            for (std::size_t j = 0; j < N; j++)
                matrix.set(i, j, value);
    }

    static std::vector<std::pair<std::string, std::string>>
    shuffle_routes (std::vector<std::string> const & source_names
        , std::vector<std::string> const dest_names);

    static std::vector<std::tuple<std::string, std::string, std::string>>
    shuffle_messages (std::vector<std::string> const & source_names
        , std::vector<std::string> const & dest_names
        , std::vector<std::string> const & messages);

private:
    std::shared_ptr<context> get_context_ptr (std::string const & name) const
    {
        return _nodes.at(name);
    }

    std::unique_ptr<node_t> create_node (std::string const & name);

    node_spec_t make_spec (std::string const & name) const
    {
        return std::make_pair(name, get_context_ptr(name)->index);
    }

    node_spec_t make_spec (node_id id) const
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
