////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../tools.hpp"
#include "mesh_network.hpp"
#include <pfs/synchronized.hpp>
#include <pfs/netty/startup.hpp>

// =================================================================================================
// Legend
// -------------------------------------------------------------------------------------------------
// A0, A1, B0, B1, C0, C1, D0, D1 - regular nodes (nodes)
// a, b, c, d - gateway nodes (gateways)
//
// =================================================================================================
// Test scheme
// -------------------------------------------------------------------------------------------------
//                     B0   B1
//                      |   |
//                      +---+
//                        |
//                 +----- b -----+
//   A0-----+      |             |      +-----C0
//          |----- a ----------- c -----|
//   A1-----+      |             |      +-----C1
//                 +----- d -----+
//                        |
//                      +---+
//                      |   |
//                     D0   D1
//

using mesh_network_t = test::meshnet::network<node_pool_t>;

std::atomic_int g_channels_established_counter {0};
pfs::synchronized<bit_matrix<12>> g_route_matrix;
pfs::synchronized<bit_matrix<12>> g_message_matrix;
std::string g_text;

static void sigterm_handler (int sig)
{
    MESSAGE("Force interrupt all nodes by signal: ", sig);
    mesh_network_t::instance()->interrupt_all();
}

TEST_CASE("messaging") {

    netty::startup_guard netty_startup;

    mesh_network_t net {
        "a", "b", "c", "d", "A0", "A1", "B0", "B1", "C0", "C1", "D0", "D1"
    };

    net.on_channel_established = [] (std::string const & source_name, std::string const & target_name
        , bool /*is_gateway*/)
    {
        LOGD(TAG, "Channel established {:>2} <--> {:>2}", source_name, target_name);
        ++g_channels_established_counter;
    };

    net.on_channel_destroyed = [] (std::string const & source_name, std::string const & target_name)
    {
        LOGD(TAG, "{}: Channel destroyed with {}", source_name, target_name);
    };

    net.on_route_ready = [] (std::string const & source_name, std::string const & target_name
        , std::vector<node_id> gw_chain, std::size_t source_index, std::size_t target_index)
    {
        g_route_matrix.wlock()->set(source_index, target_index, true);
    };

    net.on_data_received = [] (std::string const & receiver_name, std::string const & sender_name
        , int priority, std::vector<char> bytes, std::size_t source_index, std::size_t target_index)
    {
        LOGD(TAG, "Message received by {} from {}", receiver_name, sender_name);

        std::string text(bytes.data(), bytes.size());

        REQUIRE_EQ(text, g_text);

        // fmt::println(text);

        g_message_matrix.wlock()->set(source_index, target_index, true);
    };

    constexpr bool BEHIND_NAT = true;
    g_text = tools::random_text();

    // Connect gateways
    net.connect_host("a", "b");
    net.connect_host("a", "c");
    net.connect_host("a", "d");

    net.connect_host("b", "a");
    net.connect_host("b", "c");

    net.connect_host("c", "a");
    net.connect_host("c", "b");
    net.connect_host("c", "d");

    net.connect_host("d", "a");
    net.connect_host("d", "c");

    net.connect_host("A0", "a", BEHIND_NAT);
    net.connect_host("A1", "a", BEHIND_NAT);

    net.connect_host("B0", "b", BEHIND_NAT);
    net.connect_host("B1", "b", BEHIND_NAT);

    net.connect_host("C0", "c", BEHIND_NAT);
    net.connect_host("C1", "c", BEHIND_NAT);

    net.connect_host("D0", "d", BEHIND_NAT);
    net.connect_host("D1", "d", BEHIND_NAT);

    net.connect_host("A0", "A1");
    net.connect_host("A1", "A0");
    net.connect_host("B0", "B1");
    net.connect_host("B1", "B0");
    net.connect_host("C0", "C1");
    net.connect_host("C1", "C0");
    net.connect_host("D0", "D1");
    net.connect_host("D1", "D0");

    tools::signal_guard signal_guard {SIGINT, sigterm_handler};

    net.run_all();

    REQUIRE(tools::wait_atomic_counter(g_channels_established_counter, 34));
    REQUIRE(tools::wait_matrix_count(g_route_matrix, 132));
    CHECK(tools::print_matrix_with_check(*g_route_matrix.rlock(), {"a", "b", "c", "d"
        , "A0", "A1", "B0", "B1", "C0", "C1", "D0", "D1"}));

    net.send("A0", "B1", g_text);
    net.send("B1", "D1", g_text);
    net.send("D0", "A0", g_text);
    net.send("D0", "A1", g_text);
    net.send("D0", "B0", g_text);
    net.send("D0", "B1", g_text);
    net.send("D0", "C0", g_text);
    net.send("D0", "C1", g_text);
    net.send("D0", "D1", g_text);

    REQUIRE(tools::wait_matrix_count(g_message_matrix, 9));

    net.interrupt_all();
    net.join_all();
}
