////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define NETTY__TESTS_USE_MESHNET_NODE_POOL_RD
#include "../doctest.h"
#include "../tools.hpp"
#include "mesh_network.hpp"
#include <pfs/synchronized.hpp>
#include <pfs/netty/startup.hpp>

// =================================================================================================
// Legend
// -------------------------------------------------------------------------------------------------
// A0, C0 - regular nodes (nodes)
// a, b, c - gateway nodes (gateways)
//
// =================================================================================================
// Test scheme
// -------------------------------------------------------------------------------------------------
//  A0 ---- a ---- b ---- c ---- C0
//

using mesh_network_t = test::meshnet::network<reliable_node_pool_t>;

std::atomic_int g_message_delivered_counter {0};
pfs::synchronized<bit_matrix<5>> g_route_matrix;
pfs::synchronized<bit_matrix<5>> g_receiver_ready_matrix;
std::string g_text0, g_text1, g_text2;

static void sigterm_handler (int sig)
{
    MESSAGE("Force interrupt all nodes by signal: ", sig);
    mesh_network_t::instance()->interrupt_all();
}

TEST_CASE("sync delivery") {
    netty::startup_guard netty_startup;

    mesh_network_t net { "a", "b", "c", "A0", "C0" };

    net.on_route_ready = [] (std::string const & source_name, std::string const & target_name, std::uint16_t hops
        , std::size_t source_index, std::size_t target_index)
    {
        g_route_matrix.wlock()->set(source_index, target_index, true);
    };

    net.on_receiver_ready = [] (std::string const & source_name, std::string const & receiver_name
        , std::size_t source_index, std::size_t receiver_index)
    {
        LOGD(TAG, "{}: Receiver ready: {}", source_name, receiver_name);
        g_receiver_ready_matrix.wlock()->set(source_index, receiver_index, true);
    };

    net.on_message_delivered = [] (std::string const & source_name, std::string const & receiver_name
        , std::string const & msgid)
    {
        g_message_delivered_counter++;
        LOGD(TAG, "{}: Message delivered to: {}", source_name, receiver_name);
    };


    constexpr bool BEHIND_NAT = true;
    g_text0 = tools::random_text();
    g_text1 = tools::random_text();
    g_text2 = tools::random_text();

    // Connect gateways
    net.connect_host("a", "b");
    net.connect_host("b", "a");
    net.connect_host("b", "c");
    net.connect_host("c", "b");

    net.connect_host("A0", "a", BEHIND_NAT);
    net.connect_host("C0", "c", BEHIND_NAT);

    tools::signal_guard signal_guard {SIGINT, sigterm_handler};

    net.run_all();
    CHECK(tools::wait_matrix_count(g_route_matrix, 20));
    CHECK(tools::print_matrix_with_check(*g_route_matrix.rlock(), {"a", "b", "c", "A0", "C0"}));

    net.send("A0", "C0", g_text0);
    net.send("A0", "C0", g_text1);
    net.send("A0", "C0", g_text2);

    CHECK(tools::wait_matrix_count(g_receiver_ready_matrix, 1));
    CHECK(tools::wait_atomic_counter(g_message_delivered_counter, 2));

    net.interrupt_all();
    net.join_all();
}
