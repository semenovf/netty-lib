////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.06.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
// FIXME UNCOMMENT
// #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
// #include "../doctest.h"
#include "../tools.hpp"
#include "mesh_network.hpp"
#include <pfs/synchronized.hpp>
#include <pfs/netty/startup.hpp>
#include <atomic>
#include <memory>

using mesh_network_t = test::meshnet::network<node_pool_t>;

constexpr bool BEHIND_NAT = true;

#ifdef DOCTEST_VERSION
// See https://github.com/doctest/doctest/issues/345
inline char const * current_doctest_name ()
{
    return doctest::detail::g_cs->currentTest->m_name;
}
#endif

static void sigterm_handler (int sig)
{
    LOGD(TAG, "Force interrupt all nodes by signal: ", sig);
    mesh_network_t::instance()->interrupt_all();
}

auto on_channel_destroyed = [] (std::string const & source_name, std::string const & target_name)
{
    LOGD(TAG, "{}: Channel destroyed with {}", source_name, target_name);
};

// FIXME REMOVE
int main ()
{
    netty::startup_guard netty_startup;
    std::atomic_int id_duplication_flag {0};

    mesh_network_t net { "A0", "A0_dup" };

    net.on_duplicate_id = [& id_duplication_flag] (std::string const & source_name
        , std::string const & target_name, netty::socket4_addr saddr)
    {
        LOGE(TAG, "{}: Node ID duplication with: {} ({})", source_name, target_name, to_string(saddr));
        ++id_duplication_flag;
    };

    net.connect_host("A0", "A0_dup", BEHIND_NAT);

    tools::signal_guard signal_guard {SIGINT, sigterm_handler};

    net.run_all();
    tools::wait_atomic_counter(id_duplication_flag, 2);
    LOGD(TAG, "=== INTERRUPT ===");
    net.interrupt_all();
    LOGD(TAG, "=== JOIN ===");
    net.join_all();
    LOGD(TAG, "=== EXIT ===");

    return 0;
}

#if 0 // FIXME REMOVE #if
TEST_CASE("handshake behind NAT") {
    LOGD(TAG, "==========================================");
    LOGD(TAG, "= TEST CASE: {}", current_doctest_name());
    LOGD(TAG, "==========================================");

    netty::startup_guard netty_startup;
    std::atomic_int channel_established_flag {0};

    mesh_network_t net { "A0", "B0" };

    net.on_channel_established = [& channel_established_flag] (std::string const & source_name
        , std::string const & target_name, bool)
    {
        LOGD(TAG, "Channel established {:>2} <--> {:>2}", source_name, target_name);
        ++channel_established_flag;
    };

    net.on_channel_destroyed = on_channel_destroyed;
    net.connect_host("A0", "B0", BEHIND_NAT);

    tools::signal_guard signal_guard {SIGINT, sigterm_handler};

    net.run_all();
    CHECK(tools::wait_atomic_counter(channel_established_flag, 2));
    net.interrupt_all();
    net.join_all();
}
#endif

#if 0 // FIXME REMOVE #if
TEST_CASE("duplication behind NAT") {
    LOGD(TAG, "==========================================");
    LOGD(TAG, "= TEST CASE: {}", current_doctest_name());
    LOGD(TAG, "==========================================");

    netty::startup_guard netty_startup;
    std::atomic_int id_duplication_flag {0};

    mesh_network_t net { "A0", "A0_dup" };

    net.on_duplicate_id = [& id_duplication_flag] (std::string const & source_name
        , std::string const & target_name, netty::socket4_addr saddr)
    {
        LOGE(TAG, "{}: Node ID duplication with: {} ({})", source_name, target_name, to_string(saddr));
        ++id_duplication_flag;
    };

    net.connect_host("A0", "A0_dup", BEHIND_NAT);

    tools::signal_guard signal_guard {SIGINT, sigterm_handler};

    net.run_all();
    CHECK(tools::wait_atomic_counter(id_duplication_flag, 2));
    LOGD(TAG, "=== INTERRUPT ===");
    net.interrupt_all();
    LOGD(TAG, "=== JOIN ===");
    net.join_all();
    LOGD(TAG, "=== EXIT ===");
}
#endif

#if 0 // FIXME REMOVE #if
TEST_CASE("single link handshake") {
    LOGD(TAG, "==========================================");
    LOGD(TAG, "= TEST CASE: {}", current_doctest_name());
    LOGD(TAG, "==========================================");

    netty::startup_guard netty_startup;
    std::atomic_int channel_established_flag {0};

    mesh_network_t net { "A0", "B0" };

    net.on_channel_established = [& channel_established_flag] (std::string const & source_name
        , std::string const & target_name, bool)
    {
        LOGD(TAG, "Channel established {:>2} <--> {:>2}", source_name, target_name);
        ++channel_established_flag;
    };

    net.on_channel_destroyed = on_channel_destroyed;
    net.connect_host("A0", "B0");
    net.connect_host("B0", "A0");

    tools::signal_guard signal_guard {SIGINT, sigterm_handler};

    net.run_all();
    CHECK(tools::wait_atomic_counter(channel_established_flag, 2));
    net.interrupt_all();
    net.join_all();
}
#endif
