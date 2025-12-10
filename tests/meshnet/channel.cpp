////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.06.04 Initial version (handshake.cpp).
//      2025.12.08 Refactored with new version of `mesh_network`.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../tools.hpp"
#include "mesh_network.hpp"
#include <pfs/lorem/wait_atomic_counter.hpp>

// =================================================================================================
// Legend
// -------------------------------------------------------------------------------------------------
// A0, A1 - regular nodes (nodes)
// a - gateway node (gateway)
//
// =================================================================================================
// Scheme 1
// -------------------------------------------------------------------------------------------------
// A0---A1
//
// =================================================================================================
// Scheme 2 (behind NAT)
// -------------------------------------------------------------------------------------------------
// A0---a
//

constexpr bool BEHIND_NAT = true;

TEST_CASE("scheme 1") {
    LOGD(TAG, "==========================================");
    LOGD(TAG, "= TEST CASE: {}", tools::current_doctest_name());
    LOGD(TAG, "==========================================");

    lorem::wait_atomic_counter8 channel_established_counter {2};
    lorem::wait_atomic_counter8 channel_destroyed_counter {2};

    mesh_network net { "A0", "A1" };

    net.on_channel_established = [&] (std::string const & source_name
        , meshnet_ns::node_index_t, std::string const & peer_name, bool)
    {
        LOGD(TAG, "Channel established {:>2} <--> {:>2}", source_name, peer_name);
        ++channel_established_counter;
    };

    net.on_channel_destroyed = [&] (std::string const & source_name, std::string const & peer_name)
    {
        LOGD(TAG, "Channel destroyed {:>2} <--> {:>2}", source_name, peer_name);
        ++channel_destroyed_counter;
    };

    net.set_scenario([&] () {
        CHECK(channel_established_counter());
        net.disconnect("A0", "A1");
        CHECK(channel_destroyed_counter());
        net.interrupt_all();
    });

    net.listen_all();
    net.connect("A0", "A1");
    net.connect("A1", "A0");
    net.run_all();
}

TEST_CASE("scheme 2") {
    LOGD(TAG, "==========================================");
    LOGD(TAG, "= TEST CASE: {}", tools::current_doctest_name());
    LOGD(TAG, "==========================================");

    lorem::wait_atomic_counter8 channel_established_counter {2};
    lorem::wait_atomic_counter8 channel_destroyed_counter {2};

    mesh_network net { "A0", "a" };

    net.on_channel_established = [&] (std::string const & source_name
        , meshnet_ns::node_index_t, std::string const & peer_name, bool)
    {
        LOGD(TAG, "Channel established {:>2} <--> {:>2}", source_name, peer_name);
        ++channel_established_counter;
    };

    net.on_channel_destroyed = [&] (std::string const & source_name, std::string const & peer_name)
    {
        LOGD(TAG, "Channel destroyed {:>2} <--> {:>2}", source_name, peer_name);
        ++channel_destroyed_counter;
    };

    net.set_scenario([&] () {
        CHECK(channel_established_counter());
        net.disconnect("A0", "a");
        CHECK(channel_destroyed_counter());
        net.interrupt_all();
    });

    net.listen_all();
    net.connect("A0", "a", BEHIND_NAT);
    net.run_all();
}
