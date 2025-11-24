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

#if 1
TEST_CASE("simple delivery") {
    LOGD(TAG, "==========================================");
    LOGD(TAG, "= TEST CASE: {}", tools::current_doctest_name());
    LOGD(TAG, "==========================================");

    netty::startup_guard netty_startup;

    std::atomic_int route_ready_counter {0};
    std::atomic_int message_received_counter {0};
    mesh_network_t net { "A0", "A1" };

    net.on_route_ready = [& route_ready_counter] (std::string const &, std::string const &
        , std::vector<node_id>, std::size_t, std::size_t)
    {
        route_ready_counter++;
    };

    net.on_receiver_ready = [] (std::string const & source_name, std::string const & receiver_name
        , std::size_t source_index, std::size_t receiver_index)
    {
        LOGD(TAG, "{}: Receiver ready: {}", source_name, receiver_name);
        // g_receiver_ready_matrix.wlock()->set(source_index, receiver_index, true);
    };

    net.on_message_delivered = [] (std::string const & source_name, std::string const & receiver_name
        , std::string const & msgid)
    {
        // g_message_delivered_counter++;
        LOGD(TAG, "{}: Message delivered to: {}", source_name, receiver_name);
    };

    net.on_message_received = [& message_received_counter] (std::string const & source_name
        , std::string const & sender_name, std::string const & msgid, archive_t msg
        , std::size_t, std::size_t)
    {
        LOGD(TAG, "{}: Message received: {}: {}: {} bytes", source_name, sender_name, msgid
            , msg.size());
        message_received_counter++;
    };

    net.on_message_receiving_progress = [] (std::string const & source_name
        , std::string const & sender_name, std::string const & msgid
        , std::size_t received_size, std::size_t total_size)
    {
        LOGD(TAG, "{}: Message progress from: {}: {}: {}/{} ({} %)", source_name, sender_name, msgid
            , received_size, total_size, static_cast<std::size_t>(100 * (1.f * received_size)/total_size));
    };

    auto text = tools::random_large_text();

    net.connect_host("A0", "A1");
    net.connect_host("A1", "A0");

    tools::signal_guard signal_guard {SIGINT, sigterm_handler};

    net.run_all();
    CHECK(tools::wait_atomic_counter(route_ready_counter, 1));

    LOGD(TAG, "Send text: {} bytes", text.size());
    net.send_message("A0", "A1", text, 0);

    CHECK(tools::wait_atomic_counter(message_received_counter, 1));

    net.interrupt_all();
    net.join_all();
}
#endif

#if 1
TEST_CASE("delivery") {
    LOGD(TAG, "==========================================");
    LOGD(TAG, "= TEST CASE: {}", tools::current_doctest_name());
    LOGD(TAG, "==========================================");

    netty::startup_guard netty_startup;

    mesh_network_t net { "a", "b", "c", "A0", "C0" };

    net.on_route_ready = [] (std::string const & source_name, std::string const & target_name
        , std::vector<node_id> /*gw_chain*/, std::size_t source_index, std::size_t target_index)
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
    g_text0 = tools::random_large_text();
    g_text1 = tools::random_large_text();
    g_text2 = tools::random_large_text();

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

    net.send_message("A0", "C0", g_text0, 0);

    if (node_t::output_priority_count() > 1)
        net.send_message("A0", "C0", g_text1, 1);
    else
        net.send_message("A0", "C0", g_text1, 0);

    if (node_t::output_priority_count() > 2)
        net.send_message("A0", "C0", g_text2, 2);
    else
        net.send_message("A0", "C0", g_text2, 0);

    CHECK(tools::wait_matrix_count(g_receiver_ready_matrix, 1));
    CHECK(tools::wait_atomic_counter(g_message_delivered_counter, 3));

    net.interrupt_all();
    net.join_all();
}
#endif
