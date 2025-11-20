////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.11 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../tools.hpp"
#include "mesh_network.hpp"
#include <pfs/synchronized.hpp>
#include <pfs/netty/startup.hpp>
#include <atomic>
#include <memory>

// =================================================================================================
// Legend
// -------------------------------------------------------------------------------------------------
// A0, A1, B0, B1, C0, C1, D0, D1 - regular nodes (nodes)
// a, b, c, d - gateway nodes (gateways)
//
// =================================================================================================
// Test scheme 1
// -------------------------------------------------------------------------------------------------
//  A0----- a -----B0
//
// =================================================================================================
// Test scheme 2
// -------------------------------------------------------------------------------------------------
//   A0-----+             +-----B0
//          |----- a -----|
//   A1-----+             +-----B1
//
// =================================================================================================
// Test scheme 3
// -------------------------------------------------------------------------------------------------
//  A0----- a ----------- b -----B0
//
// =================================================================================================
// Test scheme 4
// -------------------------------------------------------------------------------------------------
//   A0-----+                           +-----B0
//          |----- a ----------- b -----|
//   A1-----+                           +-----B1
//
// =================================================================================================
// Test scheme 5
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
#define ITERATION_COUNT 10

#define TEST_SCHEME_1_ENABLED 1
#define TEST_SCHEME_2_ENABLED 1
#define TEST_SCHEME_3_ENABLED 1
#define TEST_SCHEME_4_ENABLED 1
#define TEST_SCHEME_5_ENABLED 1

using mesh_network_t = test::meshnet::network<node_pool_t>;

constexpr bool BEHIND_NAT = true;

// See https://github.com/doctest/doctest/issues/345
inline char const * current_doctest_name ()
{
    return doctest::detail::g_cs->currentTest->m_name;
}

#define START_TEST_MESSAGE MESSAGE("START Test: ", std::string(current_doctest_name()));
#define END_TEST_MESSAGE MESSAGE("END Test: ", std::string(current_doctest_name()));

int g_current_scheme_index = 0;
std::atomic_int g_channels_established_counter {0};

pfs::synchronized<bit_matrix<3>> g_route_matrix_1;
pfs::synchronized<bit_matrix<5>> g_route_matrix_2;
pfs::synchronized<bit_matrix<4>> g_route_matrix_3;
pfs::synchronized<bit_matrix<6>> g_route_matrix_4;
pfs::synchronized<bit_matrix<12>> g_route_matrix_5;

static void sigterm_handler (int sig)
{
    MESSAGE("Force interrupt all nodes by signal: ", sig);
    mesh_network_t::instance()->interrupt_all();
}

static void matrix_set (std::size_t row, std::size_t col, bool value = true)
{
    switch (g_current_scheme_index) {
        case 1:
            g_route_matrix_1.wlock()->set(row, col, value);
            break;
        case 2:
            g_route_matrix_2.wlock()->set(row, col, value);
            break;
        case 3:
            g_route_matrix_3.wlock()->set(row, col, value);
            break;
        case 4:
            g_route_matrix_4.wlock()->set(row, col, value);
            break;
        case 5:
            g_route_matrix_5.wlock()->set(row, col, value);
            break;
        default:
            REQUIRE((g_current_scheme_index > 0 && g_current_scheme_index < 6));
            break;
    }
}

auto on_channel_established = [] (std::string const & source_name, std::string const & target_name
    , bool /*is_gateway*/)
{
    LOGD(TAG, "Channel established {:>2} <--> {:>2}", source_name, target_name);
    ++g_channels_established_counter;
};

auto on_channel_destroyed = [] (std::string const & source_name, std::string const & target_name)
{
    LOGD(TAG, "{}: Channel destroyed with {}", source_name, target_name);
};

auto on_node_alive = [] (std::string const & source_name, std::string const & target_name)
{
    LOGD(TAG, "{}: Node alive: {}", source_name, target_name);
};

auto on_node_expired = [] (std::string const & source_name, std::string const & target_name)
{
    LOGD(TAG, "{}: Node expired: {}", source_name, target_name);
};

auto on_route_ready = [] (std::string const & source_name, std::string const & target_name
    , std::vector<node_id> gw_chain, std::size_t source_index, std::size_t target_index)
{
    auto hops = gw_chain.size();

    if (hops == 0) {
        // Gateway is this node when direct access
        LOGD(TAG, "{}: " LGREEN "Route ready" END_COLOR ": {}->{} (" LGREEN "direct access" END_COLOR ")"
            , source_name, source_name, target_name);
    } else {
        LOGD(TAG, "{}: " LGREEN "Route ready" END_COLOR ": {}->{} (" LGREEN "hops={}" END_COLOR ")"
            , source_name, source_name, target_name, hops);
    }

    matrix_set(source_index, target_index, true);
};

#if TEST_SCHEME_1_ENABLED
TEST_CASE("scheme 1") {
    netty::startup_guard netty_startup;
    g_current_scheme_index = 1;
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        g_channels_established_counter = 0;
        g_route_matrix_1.wlock()->reset();

        mesh_network_t net { "a", "A0", "B0" };

        net.on_channel_established = on_channel_established;
        net.on_channel_destroyed = on_channel_destroyed;
        net.on_node_alive = on_node_alive;
        net.on_node_expired = on_node_expired;
        net.on_route_ready = on_route_ready;

        net.connect_host("A0", "a", BEHIND_NAT);
        net.connect_host("B0", "a", BEHIND_NAT);

        tools::signal_guard signal_guard {SIGINT, sigterm_handler};

        net.run_all();
        REQUIRE(tools::wait_atomic_counter(g_channels_established_counter, 4));
        REQUIRE(tools::wait_matrix_count(g_route_matrix_1, 6));
        net.interrupt_all();
        net.join_all();

        CHECK(tools::print_matrix_with_check(*g_route_matrix_1.rlock(), {"a", "A0", "B0"}));

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_2_ENABLED
TEST_CASE("scheme 2") {
    netty::startup_guard netty_startup;
    g_current_scheme_index = 2;
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        g_channels_established_counter = 0;
        g_route_matrix_2.wlock()->reset();

        mesh_network_t net { "a", "A0", "A1", "B0", "B1" };

        net.on_channel_established = on_channel_established;
        net.on_channel_destroyed = on_channel_destroyed;
        net.on_node_alive = on_node_alive;
        net.on_node_expired = on_node_expired;
        net.on_route_ready = on_route_ready;

        net.connect_host("A0", "a", BEHIND_NAT);
        net.connect_host("A1", "a", BEHIND_NAT);

        net.connect_host("B0", "a", BEHIND_NAT);
        net.connect_host("B1", "a", BEHIND_NAT);

        net.connect_host("A0", "A1");
        net.connect_host("A1", "A0");
        net.connect_host("B0", "B1");
        net.connect_host("B1", "B0");

        tools::signal_guard signal_guard {SIGINT, sigterm_handler};

        net.run_all();
        REQUIRE(tools::wait_atomic_counter(g_channels_established_counter, 12));
        REQUIRE(tools::wait_matrix_count(g_route_matrix_2, 20));
        net.interrupt_all();
        net.join_all();

        CHECK(tools::print_matrix_with_check(*g_route_matrix_2.rlock(), {"a", "A0", "A1", "B0", "B1" }));

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_3_ENABLED
TEST_CASE("scheme 3") {
    netty::startup_guard netty_startup;
    g_current_scheme_index = 3;
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        g_channels_established_counter = 0;
        g_route_matrix_3.wlock()->reset();

        mesh_network_t net { "a", "b", "A0", "B0" };

        net.on_channel_established = on_channel_established;
        net.on_channel_destroyed = on_channel_destroyed;
        net.on_node_alive = on_node_alive;
        net.on_node_expired = on_node_expired;
        net.on_route_ready = on_route_ready;

        // Connect gateways
        net.connect_host("a", "b");
        net.connect_host("b", "a");

        net.connect_host("A0", "a", BEHIND_NAT);
        net.connect_host("B0", "b", BEHIND_NAT);

        tools::signal_guard signal_guard {SIGINT, sigterm_handler};

        net.run_all();
        REQUIRE(tools::wait_atomic_counter(g_channels_established_counter, 6));
        REQUIRE(tools::wait_matrix_count(g_route_matrix_3, 12));
        net.interrupt_all();
        net.join_all();

        CHECK(tools::print_matrix_with_check(*g_route_matrix_3.rlock(), {"a", "b", "A0", "B0"}));

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_4_ENABLED
TEST_CASE("scheme 4") {
    netty::startup_guard netty_startup;
    g_current_scheme_index = 4;
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        g_channels_established_counter = 0;
        g_route_matrix_4.wlock()->reset();

        mesh_network_t net { "a", "b", "A0", "A1", "B0", "B1" };

        net.on_channel_established = on_channel_established;
        net.on_channel_destroyed = on_channel_destroyed;
        net.on_node_alive = on_node_alive;
        net.on_node_expired = on_node_expired;
        net.on_route_ready = on_route_ready;

        // Connect gateways
        net.connect_host("a", "b");
        net.connect_host("b", "a");

        net.connect_host("A0", "a", BEHIND_NAT);
        net.connect_host("A1", "a", BEHIND_NAT);

        net.connect_host("B0", "b", BEHIND_NAT);
        net.connect_host("B1", "b", BEHIND_NAT);

        net.connect_host("A0", "A1");
        net.connect_host("A1", "A0");
        net.connect_host("B0", "B1");
        net.connect_host("B1", "B0");

        tools::signal_guard signal_guard {SIGINT, sigterm_handler};

        net.run_all();
        REQUIRE(tools::wait_atomic_counter(g_channels_established_counter, 14));
        REQUIRE(tools::wait_matrix_count(g_route_matrix_4, 30));
        net.interrupt_all();
        net.join_all();

        CHECK(tools::print_matrix_with_check(*g_route_matrix_4.rlock(), {"a", "b", "A0", "A1", "B0", "B1"}));

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_5_ENABLED
TEST_CASE("scheme 5") {
    netty::startup_guard netty_startup;
    g_current_scheme_index = 5;
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        g_channels_established_counter = 0;
        g_route_matrix_5.wlock()->reset();

        mesh_network_t net { "a", "b", "c", "d"
            , "A0", "A1", "B0", "B1", "C0", "C1", "D0", "D1" };

        net.on_channel_established = on_channel_established;
        net.on_channel_destroyed = on_channel_destroyed;
        net.on_node_alive = on_node_alive;
        net.on_node_expired = on_node_expired;
        net.on_route_ready = on_route_ready;

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
        REQUIRE(tools::wait_matrix_count(g_route_matrix_5, 132));
        net.interrupt_all();
        net.join_all();

        CHECK(tools::print_matrix_with_check(*g_route_matrix_5.rlock(), {"a", "b", "c", "d"
            , "A0", "A1", "B0", "B1", "C0", "C1", "D0", "D1"}));

        END_TEST_MESSAGE
    }
}
#endif
