////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.12.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../tools.hpp"
#include "mesh_network.hpp"
#include <pfs/lorem/wait_atomic_counter.hpp>
#include <pfs/lorem/wait_bitmatrix.hpp>

// =================================================================================================
// Legend
// -------------------------------------------------------------------------------------------------
// A0, A0_dup - regular nodes (nodes)
//
// =================================================================================================
// Scheme 1
// -------------------------------------------------------------------------------------------------
// A0---A0_dup
//
// =================================================================================================
// Scheme 2 (behind NAT)
// -------------------------------------------------------------------------------------------------
// A0---A0_dup
//
// =================================================================================================
// Scheme 3 (with gateway)
// -------------------------------------------------------------------------------------------------
// A0---a---b---A0_dup
//

#define TEST_SCHEME_1_ENABLED 1
#define TEST_SCHEME_2_ENABLED 1
#define TEST_SCHEME_3_ENABLED 1

constexpr bool BEHIND_NAT = true;

#if TEST_SCHEME_1_ENABLED
TEST_CASE("scheme 1") {
    START_TEST_MESSAGE

    // Why 4? -------------------------------------------
    // 1. A0--->A0_dup (request)                        |
    // 2. A0<---A0_dup (response)                       |
    // 3. A0_dup--->A0 (request)                        |
    // 4. A0_dup<---A0 (response)                       v
    lorem::wait_atomic_counter8 duplication_id_counter {4};

    mesh_network net { "A0", "A0_dup" };

    net.on_duplicate_id = [&] (node_spec_t const & source, node_spec_t const & peer
        , std::string const & host_addr)
    {
        LOGE(TAG, "{}: Node ID duplication with: {} ({})", source.first, peer.first, host_addr);
        ++duplication_id_counter;
    };

    net.set_scenario([&] () {
        CHECK(duplication_id_counter.wait());
        net.interrupt_all();
    });

    net.listen_all();
    net.connect("A0", "A0_dup");
    net.connect("A0_dup", "A0");
    net.run_all();

    END_TEST_MESSAGE
}
#endif

#if TEST_SCHEME_2_ENABLED
TEST_CASE("scheme 2") {
    START_TEST_MESSAGE

    // Why 2? -------------------------------------------
    // 1. A0--->A0_dup (request)                        |
    // 2. A0<---A0_dup (response)                       v
    lorem::wait_atomic_counter8 duplication_id_counter {2};

    mesh_network net { "A0", "A0_dup" };

    net.on_duplicate_id = [&] (node_spec_t const & source, node_spec_t const & peer
        , std::string const & host_addr)
    {
        LOGE(TAG, "{}: Node ID duplication with: {} ({})", source.first, peer.first, host_addr);
        ++duplication_id_counter;
    };

    net.set_scenario([&] () {
        CHECK(duplication_id_counter.wait());
        net.interrupt_all();
    });

    net.listen_all();
    net.connect("A0", "A0_dup", BEHIND_NAT);
    net.run_all();

    END_TEST_MESSAGE
}
#endif

#if TEST_SCHEME_3_ENABLED
TEST_CASE("scheme 3") {
    START_TEST_MESSAGE

    std::atomic_bool isA0_duplicated {false};
    std::atomic_bool isA0_dup_duplicated {false};

    mesh_network net { "a", "b", "A0", "A0_dup" };

    net.on_duplicate_id = [&] (node_spec_t const & source, node_spec_t const & peer
        , std::string const & host_addr)
    {
        LOGE(TAG, "{}: Node ID duplication with: {} ({})", source.first, peer.first, host_addr);
        // ++duplication_id_counter;
        if (source.first == "A0")
            isA0_duplicated = true;

        if (source.first == "A0_dup")
            isA0_dup_duplicated = true;
    };

    net.set_scenario([&] () {
        std::this_thread::sleep_for(std::chrono::seconds{2});
        CHECK((isA0_duplicated && isA0_dup_duplicated));
        net.interrupt_all();
    });

    net.listen_all();
    net.connect("a", "b");
    net.connect("b", "a");
    net.connect("A0", "a", BEHIND_NAT);
    net.connect("A0_dup", "b", BEHIND_NAT);

    net.run_all();

    END_TEST_MESSAGE
}
#endif
