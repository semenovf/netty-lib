////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.14 Initial version.
//      2025.05.12 Fixed with new test environment: `test::meshnet::network`.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../tools.hpp"
#include "mesh_network.hpp"
#include <pfs/netty/startup.hpp>
#include <atomic>
#include <memory>

using mesh_network_t = test::meshnet::network<node_pool_t>;

std::atomic_int g_duplication_counter {0};

static void sigterm_handler (int sig)
{
    MESSAGE("Force interrupt all nodes by signal: ", sig);
    mesh_network_t::instance()->interrupt_all();
}

TEST_CASE("duplication") {

    netty::startup_guard netty_startup;

    mesh_network_t net { "A0", "A0_dup" };

    net.on_duplicated = [] (std::string const & source_name, std::string const & target_name
        , netty::socket4_addr saddr)
    {
        LOGE(TAG, "{}: Node ID duplication with: {} ({})", source_name, target_name, to_string(saddr));
        ++g_duplication_counter;
    };

    net.connect_host("A0", "A0_dup");
    net.connect_host("A0_dup", "A0");

    tools::signal_guard signal_guard {SIGINT, sigterm_handler};

    net.run_all();
    CHECK(tools::wait_atomic_counter(g_duplication_counter, 2));
    net.interrupt_all();
    net.join_all();
}
