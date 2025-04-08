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
#include "tools.hpp"
#include <pfs/fmt.hpp>
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

using namespace netty::patterns;

constexpr bool BEHIND_NAT = true;

// See https://github.com/doctest/doctest/issues/345
inline char const * current_doctest_name ()
{
    return doctest::detail::g_cs->currentTest->m_name;
}

#define START_TEST_MESSAGE MESSAGE("START Test: ", std::string(current_doctest_name()));
#define END_TEST_MESSAGE MESSAGE("END Test: ", std::string(current_doctest_name()));

int g_current_scheme_index = 0;

tools::mesh_network * g_mesh_network_ptr = nullptr;
std::atomic_int g_channels_established_counter {0};

using stub_matrix_type = pfs::synchronized<bit_matrix<1>>;
pfs::synchronized<bit_matrix<3>> g_route_matrix_1;
pfs::synchronized<bit_matrix<5>> g_route_matrix_2;
pfs::synchronized<bit_matrix<4>> g_route_matrix_3;
pfs::synchronized<bit_matrix<6>> g_route_matrix_4;
pfs::synchronized<bit_matrix<12>> g_route_matrix_5;

static void sigterm_handler (int sig)
{
    MESSAGE("Force interrupt: ", sig);

    if (g_mesh_network_ptr != nullptr) {
        MESSAGE("Force interrupt all nodes by signal: ", sig);
        g_mesh_network_ptr->interrupt_all();
    }
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

void tools::mesh_network::on_channel_established (std::string const & source_name
    , node_t::node_id_rep id_rep, bool /*is_gateway*/)
{
    LOGD(TAG, "Channel established {:>2} <--> {:>2}", source_name, node_name_by_id(id_rep));
    ++g_channels_established_counter;
}

void tools::mesh_network::on_channel_destroyed (std::string const & source_name
    , node_t::node_id_rep id_rep)
{
    LOGD(TAG, "{}: Channel destroyed with {}", source_name, node_name_by_id(id_rep));
}

void tools::mesh_network::on_node_alive (std::string const & source_name
    , node_t::node_id_rep id_rep)
{
    LOGD(TAG, "{}: Node alive: {}", source_name, node_name_by_id(id_rep));
}

void tools::mesh_network::on_node_expired (std::string const & source_name
    , node_t::node_id_rep id_rep)
{
    LOGD(TAG, "{}: Node expired: {}", source_name, node_name_by_id(id_rep));
}

void tools::mesh_network::on_route_ready (std::string const & source_name
    , node_t::node_id_rep dest_id_rep, std::uint16_t hops)
{
    if (hops == 0) {
        // Gateway is this node when direct access
        LOGD(TAG, "{}: " LGREEN "Route ready" END_COLOR ": {}->{} (" LGREEN "direct access" END_COLOR ")"
            , source_name
            , node_pool_t::node_id_traits::to_string(node_id_by_name(source_name))
            , node_pool_t::node_id_traits::to_string(dest_id_rep));
    } else {
        LOGD(TAG, "{}: " LGREEN "Route ready" END_COLOR ": {}->{} (" LGREEN "hops={}" END_COLOR ")"
            , source_name
            , node_pool_t::node_id_traits::to_string(node_id_by_name(source_name))
            , node_pool_t::node_id_traits::to_string(dest_id_rep)
            , hops);
    }

    auto row = serial_number(source_name);
    auto col = serial_number(dest_id_rep);

    matrix_set(row, col, true);
}

// TEST_CASE("duplication") {
//     START_TEST_MESSAGE
//
//     netty::startup_guard netty_startup;
//
//     create_node_pool<stub_matrix_type>("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false, 0, nullptr);
//     create_node_pool<stub_matrix_type>("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0_dup", 4201, false, 1, nullptr);
//
//     tools::connect_host(1, "A0", "A0_dup");
//
//     signal(SIGINT, tools::sigterm_handler);
//
//     tools::run_all();
//     tools::sleep(1, "Check channels duplication");
//     tools::interrupt_all();
//     tools::join_all();
//     tools::clear();
//
//     END_TEST_MESSAGE
// }

#if TEST_SCHEME_1_ENABLED
TEST_CASE("scheme 1") {
    netty::startup_guard netty_startup;
    g_current_scheme_index = 1;
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        g_channels_established_counter = 0;
        g_route_matrix_1.wlock()->reset();

        tools::mesh_network mesh_network { "a", "A0", "B0" };

        mesh_network.connect_host("A0", "a", BEHIND_NAT);
        mesh_network.connect_host("B0", "a", BEHIND_NAT);

        g_mesh_network_ptr = & mesh_network;

        tools::signal_guard signal_guard {SIGINT, sigterm_handler};

        mesh_network.run_all();
        REQUIRE(tools::wait_atomic_counter(g_channels_established_counter, 4));
        REQUIRE(tools::wait_matrix_count(g_route_matrix_1, 6));
        mesh_network.interrupt_all();
        mesh_network.join_all();

        CHECK(tools::print_matrix_with_check(*g_route_matrix_1.rlock(), {"a", "A0", "B0"}));

        g_mesh_network_ptr = nullptr;

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

        tools::mesh_network mesh_network { "a", "A0", "A1", "B0", "B1" };

        mesh_network.connect_host("A0", "a", BEHIND_NAT);
        mesh_network.connect_host("A1", "a", BEHIND_NAT);

        mesh_network.connect_host("B0", "a", BEHIND_NAT);
        mesh_network.connect_host("B1", "a", BEHIND_NAT);

        mesh_network.connect_host("A0", "A1");
        mesh_network.connect_host("A1", "A0");
        mesh_network.connect_host("B0", "B1");
        mesh_network.connect_host("B1", "B0");

        g_mesh_network_ptr = & mesh_network;

        tools::signal_guard signal_guard {SIGINT, sigterm_handler};

        mesh_network.run_all();
        REQUIRE(tools::wait_atomic_counter(g_channels_established_counter, 12));
        REQUIRE(tools::wait_matrix_count(g_route_matrix_2, 20));
        mesh_network.interrupt_all();
        mesh_network.join_all();

        CHECK(tools::print_matrix_with_check(*g_route_matrix_2.rlock(), {"a", "A0", "A1", "B0", "B1" }));

        g_mesh_network_ptr = nullptr;

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

        tools::mesh_network mesh_network { "a", "b", "A0", "B0" };

        // Connect gateways
        mesh_network.connect_host("a", "b");
        mesh_network.connect_host("b", "a");

        mesh_network.connect_host("A0", "a", BEHIND_NAT);
        mesh_network.connect_host("B0", "b", BEHIND_NAT);

        g_mesh_network_ptr = & mesh_network;

        tools::signal_guard signal_guard {SIGINT, sigterm_handler};

        mesh_network.run_all();
        REQUIRE(tools::wait_atomic_counter(g_channels_established_counter, 6));
        REQUIRE(tools::wait_matrix_count(g_route_matrix_3, 12));
        mesh_network.interrupt_all();
        mesh_network.join_all();

        CHECK(tools::print_matrix_with_check(*g_route_matrix_3.rlock(), {"a", "b", "A0", "B0"}));

        g_mesh_network_ptr = nullptr;

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

        tools::mesh_network mesh_network { "a", "b", "A0", "A1", "B0", "B1" };

        // Connect gateways
        mesh_network.connect_host("a", "b");
        mesh_network.connect_host("b", "a");

        mesh_network.connect_host("A0", "a", BEHIND_NAT);
        mesh_network.connect_host("A1", "a", BEHIND_NAT);

        mesh_network.connect_host("B0", "b", BEHIND_NAT);
        mesh_network.connect_host("B1", "b", BEHIND_NAT);

        mesh_network.connect_host("A0", "A1");
        mesh_network.connect_host("A1", "A0");
        mesh_network.connect_host("B0", "B1");
        mesh_network.connect_host("B1", "B0");

        g_mesh_network_ptr = & mesh_network;

        tools::signal_guard signal_guard {SIGINT, sigterm_handler};

        mesh_network.run_all();
        REQUIRE(tools::wait_atomic_counter(g_channels_established_counter, 14));
        REQUIRE(tools::wait_matrix_count(g_route_matrix_4, 30));
        mesh_network.interrupt_all();
        mesh_network.join_all();

        CHECK(tools::print_matrix_with_check(*g_route_matrix_4.rlock(), {"a", "b", "A0", "A1", "B0", "B1"}));

        g_mesh_network_ptr = nullptr;

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

        tools::mesh_network mesh_network { "a", "b", "c", "d"
            , "A0", "A1", "B0", "B1", "C0", "C1", "D0", "D1" };

        mesh_network.connect_host("a", "b");
        mesh_network.connect_host("a", "c");
        mesh_network.connect_host("a", "d");

        mesh_network.connect_host("b", "a");
        mesh_network.connect_host("b", "c");

        mesh_network.connect_host("c", "a");
        mesh_network.connect_host("c", "b");
        mesh_network.connect_host("c", "d");

        mesh_network.connect_host("d", "a");
        mesh_network.connect_host("d", "c");

        mesh_network.connect_host("A0", "a", BEHIND_NAT);
        mesh_network.connect_host("A1", "a", BEHIND_NAT);

        mesh_network.connect_host("B0", "b", BEHIND_NAT);
        mesh_network.connect_host("B1", "b", BEHIND_NAT);

        mesh_network.connect_host("C0", "c", BEHIND_NAT);
        mesh_network.connect_host("C1", "c", BEHIND_NAT);

        mesh_network.connect_host("D0", "d", BEHIND_NAT);
        mesh_network.connect_host("D1", "d", BEHIND_NAT);

        mesh_network.connect_host("A0", "A1");
        mesh_network.connect_host("A1", "A0");
        mesh_network.connect_host("B0", "B1");
        mesh_network.connect_host("B1", "B0");
        mesh_network.connect_host("C0", "C1");
        mesh_network.connect_host("C1", "C0");
        mesh_network.connect_host("D0", "D1");
        mesh_network.connect_host("D1", "D0");

        g_mesh_network_ptr = & mesh_network;

        tools::signal_guard signal_guard {SIGINT, sigterm_handler};

        mesh_network.run_all();
        REQUIRE(tools::wait_atomic_counter(g_channels_established_counter, 34));
        REQUIRE(tools::wait_matrix_count(g_route_matrix_5, 132));
        mesh_network.interrupt_all();
        mesh_network.join_all();

        CHECK(tools::print_matrix_with_check(*g_route_matrix_5.rlock(), {"a", "b", "c", "d"
            , "A0", "A1", "B0", "B1", "C0", "C1", "D0", "D1"}));

        g_mesh_network_ptr = nullptr;

        END_TEST_MESSAGE
    }
}
#endif
