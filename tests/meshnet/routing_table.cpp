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
#include "bit_matrix.hpp"
#include "node.hpp"
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
// Test scheme 4 (not implemented yet)
// -------------------------------------------------------------------------------------------------
//   A0-----+                           +-----B0
//          |----- a ----------- b -----|
//   A1-----+                           +-----B1
//
// =================================================================================================
// Test scheme 5 (not implemented yet)
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
#define ITERATION_COUNT 100

#define TEST_SCHEME_1_ENABLED 1
#define TEST_SCHEME_2_ENABLED 1
#define TEST_SCHEME_3_ENABLED 1
#define TEST_SCHEME_4_ENABLED 1
#define TEST_SCHEME_5_ENABLED 1

using namespace netty::patterns;

// See https://github.com/doctest/doctest/issues/345
inline char const * current_doctest_name ()
{
    return doctest::detail::g_cs->currentTest->m_name;
}

#define START_TEST_MESSAGE MESSAGE("START Test: ", std::string(current_doctest_name()));
#define END_TEST_MESSAGE MESSAGE("END Test: ", std::string(current_doctest_name()));

TEST_CASE("duplication") {
    START_TEST_MESSAGE

    netty::startup_guard netty_startup;

    tools::create_node_pool<tools::stub_matrix_type>("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false, 0, nullptr);
    tools::create_node_pool<tools::stub_matrix_type>("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0_dup", 4201, false, 1, nullptr);

    tools::connect_host(1, "A0", "A0_dup");

    signal(SIGINT, tools::sigterm_handler);

    tools::run_all();
    tools::sleep(1, "Check channels duplication");
    tools::interrupt_all();
    tools::join_all();
    tools::clear();

    END_TEST_MESSAGE
}

#if TEST_SCHEME_1_ENABLED
TEST_CASE("scheme 1") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        netty::startup_guard netty_startup;
        bool behind_nat = true;

        std::size_t serial_number = 0;

        // Create gateways
        tools::create_node_pool("01JQN2NGY47H3R81Y9SG0F0A00"_uuid, "a", 4210, true, serial_number++, & tools::s_route_matrix_1);

        // Create regular nodes
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false, serial_number++, & tools::s_route_matrix_1);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VB0"_uuid, "B0", 4221, false, serial_number++, & tools::s_route_matrix_1);

        REQUIRE_EQ(serial_number, tools::s_route_matrix_1.rlock()->rows());

        tools::connect_host(1, "A0", "a", behind_nat);
        tools::connect_host(1, "B0", "a", behind_nat);

        tools::run_all();
        REQUIRE(tools::wait_atomic_counter_limit(tools::channels_established_counter, 4));
        REQUIRE(tools::wait_matrix_count(tools::s_route_matrix_1, 6));
        tools::interrupt_all();
        tools::join_all();
        tools::print_matrix(*tools::s_route_matrix_1.rlock(), {"a", "A0", "B0"});
        tools::clear();

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_2_ENABLED
TEST_CASE("scheme 2") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        netty::startup_guard netty_startup;
        bool behind_nat = true;

        std::size_t serial_number = 0;

        // Create gateways
        tools::create_node_pool("01JQN2NGY47H3R81Y9SG0F0A00"_uuid, "a", 4210, true, serial_number++, & tools::s_route_matrix_2);

        // Create regular nodes
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false, serial_number++, & tools::s_route_matrix_2);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VA1"_uuid, "A1", 4212, false, serial_number++, & tools::s_route_matrix_2);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VB0"_uuid, "B0", 4221, false, serial_number++, & tools::s_route_matrix_2);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VB1"_uuid, "B1", 4222, false, serial_number++, & tools::s_route_matrix_2);

        REQUIRE_EQ(serial_number, tools::s_route_matrix_2.rlock()->rows());

        tools::connect_host(1, "A0", "a", behind_nat);
        tools::connect_host(1, "A1", "a", behind_nat);

        tools::connect_host(1, "B0", "a", behind_nat);
        tools::connect_host(1, "B1", "a", behind_nat);

        tools::connect_host(1, "A0", "A1");
        tools::connect_host(1, "A1", "A0");
        tools::connect_host(1, "B0", "B1");
        tools::connect_host(1, "B1", "B0");

        tools::run_all();
        REQUIRE(tools::wait_atomic_counter_limit(tools::channels_established_counter, 12));
        REQUIRE(tools::wait_matrix_count(tools::s_route_matrix_2, 20));
        tools::interrupt_all();
        tools::join_all();
        tools::print_matrix(*tools::s_route_matrix_2.rlock(), {"a", "A0", "A1", "B0", "B1" });
        tools::clear();

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_3_ENABLED
TEST_CASE("scheme 3") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        netty::startup_guard netty_startup;
        bool behind_nat = true;

        std::size_t serial_number = 0;

        // Create gateways
        tools::create_node_pool("01JQN2NGY47H3R81Y9SG0F0A00"_uuid, "a", 4210, true, serial_number++, & tools::s_route_matrix_3);
        tools::create_node_pool("01JQN2NGY47H3R81Y9SG0F0B00"_uuid, "b", 4220, true, serial_number++, & tools::s_route_matrix_3);

        // Create regular nodes
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false, serial_number++, & tools::s_route_matrix_3);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VB0"_uuid, "B0", 4221, false, serial_number++, & tools::s_route_matrix_3);

        REQUIRE_EQ(serial_number, tools::s_route_matrix_3.rlock()->rows());

        // Connect gateways
        tools::connect_host(1, "a", "b");
        tools::connect_host(1, "b", "a");

        tools::connect_host(1, "A0", "a", behind_nat);
        tools::connect_host(1, "B0", "b", behind_nat);

        tools::run_all();
        REQUIRE(tools::wait_atomic_counter_limit(tools::channels_established_counter, 6));
        REQUIRE(tools::wait_matrix_count(tools::s_route_matrix_3, 12));
        tools::interrupt_all();
        tools::join_all();
        tools::print_matrix(*tools::s_route_matrix_3.rlock(), {"a", "b", "A0", "B0"});
        tools::clear();

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_4_ENABLED
TEST_CASE("scheme 4") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        netty::startup_guard netty_startup;
        bool behind_nat = true;

        std::size_t serial_number = 0;

        // Create gateways
        tools::create_node_pool("01JQN2NGY47H3R81Y9SG0F0A00"_uuid, "a", 4210, true, serial_number++, & tools::s_route_matrix_4);
        tools::create_node_pool("01JQN2NGY47H3R81Y9SG0F0B00"_uuid, "b", 4220, true, serial_number++, & tools::s_route_matrix_4);

        // Create regular nodes
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false, serial_number++, & tools::s_route_matrix_4);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VA1"_uuid, "A1", 4212, false, serial_number++, & tools::s_route_matrix_4);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VB0"_uuid, "B0", 4221, false, serial_number++, & tools::s_route_matrix_4);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VB1"_uuid, "B1", 4222, false, serial_number++, & tools::s_route_matrix_4);

        REQUIRE_EQ(serial_number, tools::s_route_matrix_4.rlock()->rows());

        // Connect gateways
        tools::connect_host(1, "a", "b");
        tools::connect_host(1, "b", "a");

        tools::connect_host(1, "A0", "a", behind_nat);
        tools::connect_host(1, "A1", "a", behind_nat);

        tools::connect_host(1, "B0", "b", behind_nat);
        tools::connect_host(1, "B1", "b", behind_nat);

        tools::connect_host(1, "A0", "A1");
        tools::connect_host(1, "A1", "A0");
        tools::connect_host(1, "B0", "B1");
        tools::connect_host(1, "B1", "B0");

        tools::run_all();
        REQUIRE(tools::wait_atomic_counter_limit(tools::channels_established_counter, 14));
        REQUIRE(tools::wait_matrix_count(tools::s_route_matrix_4, 30));
        tools::interrupt_all();
        tools::join_all();
        tools::print_matrix(*tools::s_route_matrix_4.rlock(), {"a", "b", "A0", "A1", "B0", "B1"});
        tools::clear();

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_5_ENABLED
TEST_CASE("scheme 5") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        netty::startup_guard netty_startup;
        bool behind_nat = true;

        std::size_t serial_number = 0;

        // Create gateways
        tools::create_node_pool("01JQN2NGY47H3R81Y9SG0F0A00"_uuid, "a", 4210, true, serial_number++, & tools::s_route_matrix_5);
        tools::create_node_pool("01JQN2NGY47H3R81Y9SG0F0B00"_uuid, "b", 4220, true, serial_number++, & tools::s_route_matrix_5);
        tools::create_node_pool("01JQN2NGY47H3R81Y9SG0F0C00"_uuid, "c", 4230, true, serial_number++, & tools::s_route_matrix_5);
        tools::create_node_pool("01JQN2NGY47H3R81Y9SG0F0D00"_uuid, "d", 4240, true, serial_number++, & tools::s_route_matrix_5);

        // Create regular nodes
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false, serial_number++, & tools::s_route_matrix_5);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VA1"_uuid, "A1", 4212, false, serial_number++, & tools::s_route_matrix_5);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VB0"_uuid, "B0", 4221, false, serial_number++, & tools::s_route_matrix_5);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VB1"_uuid, "B1", 4222, false, serial_number++, & tools::s_route_matrix_5);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VC0"_uuid, "C0", 4231, false, serial_number++, & tools::s_route_matrix_5);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VC1"_uuid, "C1", 4232, false, serial_number++, & tools::s_route_matrix_5);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VD0"_uuid, "D0", 4241, false, serial_number++, & tools::s_route_matrix_5);
        tools::create_node_pool("01JQC29M6RC2EVS1ZST11P0VD1"_uuid, "D1", 4242, false, serial_number++, & tools::s_route_matrix_5);

        REQUIRE_EQ(serial_number, tools::s_route_matrix_5.rlock()->rows());

        // Connect gateways
        tools::connect_host(1, "a", "b");
        tools::connect_host(1, "a", "c");
        tools::connect_host(1, "a", "d");

        tools::connect_host(1, "b", "a");
        tools::connect_host(1, "b", "c");

        tools::connect_host(1, "c", "a");
        tools::connect_host(1, "c", "b");
        tools::connect_host(1, "c", "d");

        tools::connect_host(1, "d", "a");
        tools::connect_host(1, "d", "c");

        tools::connect_host(1, "A0", "a", behind_nat);
        tools::connect_host(1, "A1", "a", behind_nat);

        tools::connect_host(1, "B0", "b", behind_nat);
        tools::connect_host(1, "B1", "b", behind_nat);

        tools::connect_host(1, "C0", "c", behind_nat);
        tools::connect_host(1, "C1", "c", behind_nat);

        tools::connect_host(1, "D0", "d", behind_nat);
        tools::connect_host(1, "D1", "d", behind_nat);

        tools::connect_host(1, "A0", "A1");
        tools::connect_host(1, "A1", "A0");
        tools::connect_host(1, "B0", "B1");
        tools::connect_host(1, "B1", "B0");
        tools::connect_host(1, "C0", "C1");
        tools::connect_host(1, "C1", "C0");
        tools::connect_host(1, "D0", "D1");
        tools::connect_host(1, "D1", "D0");

        tools::run_all();
        REQUIRE(tools::wait_atomic_counter_limit(tools::channels_established_counter, 34));
        REQUIRE(tools::wait_matrix_count(tools::s_route_matrix_5, 132));
        tools::interrupt_all();
        tools::join_all();
        tools::print_matrix(*tools::s_route_matrix_5.rlock(), {"a", "b", "c", "d"
            , "A0", "A1", "B0", "B1", "C0", "C1", "D0", "D1"});
        tools::clear();

        END_TEST_MESSAGE
    }
}
#endif
