////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.14 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "tools.hpp"
#include <pfs/fmt.hpp>
#include <pfs/netty/startup.hpp>
#include <atomic>
#include <memory>

using namespace netty::patterns;

tools::mesh_network * g_mesh_network_ptr = nullptr;
std::atomic_int g_duplication_counter {0};

static void sigterm_handler (int sig)
{
    MESSAGE("Force interrupt: ", sig);

    if (g_mesh_network_ptr != nullptr) {
        MESSAGE("Force interrupt all nodes by signal: ", sig);
        g_mesh_network_ptr->interrupt_all();
    }
}

void tools::mesh_network::on_channel_established (std::string const & source_name
    , node_t::node_id id, bool /*is_gateway*/)
{
    LOGD(TAG, "Channel established {:>2} <--> {:>2}", source_name, node_name_by_id(id));
}

void tools::mesh_network::on_channel_destroyed (std::string const & source_name, node_t::node_id id)
{
    LOGD(TAG, "{}: Channel destroyed with {}", source_name, node_name_by_id(id));
}

void tools::mesh_network::on_duplicated (std::string const & source_name, node_t::node_id id
    , std::string const & name, netty::socket4_addr saddr)
{
    LOGE(TAG, "{}: Node ID duplication with: {} ({})", source_name, name, to_string(saddr));
    ++g_duplication_counter;
};

void tools::mesh_network::on_node_alive (std::string const & source_name
    , node_t::node_id id)
{}

void tools::mesh_network::on_node_expired (std::string const & source_name, node_t::node_id id)
{}

void tools::mesh_network::on_message_received (std::string const & /*receiver_name*/
    , node_t::node_id /*sender_id*/, int /*priority*/, std::vector<char> && /*bytes*/)
{}

void tools::mesh_network::on_route_ready (std::string const & source_name, node_t::node_id dest_id
    , std::uint16_t hops)
{}

TEST_CASE("duplication") {

    netty::startup_guard netty_startup;

    tools::mesh_network mesh_network { "A0", "A0_dup" };

    mesh_network.connect_host("A0", "A0_dup");
    mesh_network.connect_host("A0_dup", "A0");

    g_mesh_network_ptr = & mesh_network;

    tools::signal_guard signal_guard {SIGINT, sigterm_handler};

    mesh_network.run_all();
    CHECK(tools::wait_atomic_counter(g_duplication_counter, 2));
    mesh_network.interrupt_all();
    mesh_network.join_all();

    g_mesh_network_ptr = nullptr;
}
