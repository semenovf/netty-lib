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

constexpr bool BEHIND_NAT = true;

TEST_CASE("scheme 1") {
    LOGD(TAG, "==========================================");
    LOGD(TAG, "= TEST CASE: {}", tools::current_doctest_name());
    LOGD(TAG, "==========================================");

    // Why 4? -------------------------------------------
    // 1. A0--->A0_dup (request)                        |
    // 2. A0<---A0_dup (response)                       |
    // 3. A0_dup--->A0 (request)                        |
    // 4. A0_dup<---A0 (response)                       v
    lorem::wait_atomic_counter8 duplication_id_counter {4};

    mesh_network net { "A0", "A0_dup" };

    net.on_duplicate_id = [&] (node_spec_t const & source, node_spec_t const & peer
        , netty::socket4_addr saddr)
    {
        LOGE(TAG, "{}: Node ID duplication with: {} ({})", source.first, peer.first, to_string(saddr));
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
}

TEST_CASE("scheme 2") {
    LOGD(TAG, "==========================================");
    LOGD(TAG, "= TEST CASE: {}", tools::current_doctest_name());
    LOGD(TAG, "==========================================");

    // Why 2? -------------------------------------------
    // 1. A0--->A0_dup (request)                        |
    // 2. A0<---A0_dup (response)                       v
    lorem::wait_atomic_counter8 duplication_id_counter {2};

    mesh_network net { "A0", "A0_dup" };

    net.on_duplicate_id = [&] (node_spec_t const & source, node_spec_t const & peer
        , netty::socket4_addr saddr)
    {
        LOGE(TAG, "{}: Node ID duplication with: {} ({})", source.first, peer.first, to_string(saddr));
        ++duplication_id_counter;
    };

    net.set_scenario([&] () {
        CHECK(duplication_id_counter.wait());
        net.interrupt_all();
    });

    net.listen_all();
    net.connect("A0", "A0_dup", BEHIND_NAT);
    net.run_all();
}
