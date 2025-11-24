////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.11 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../tools.hpp"
#include "mesh_network.hpp"
#include <pfs/synchronized.hpp>
#include <pfs/lorem/lorem_ipsum.hpp>
#include <pfs/netty/startup.hpp>

// =================================================================================================
// Legend
// -------------------------------------------------------------------------------------------------
// A0, C0 - regular nodes (nodes)
// a, b, c, d - gateway nodes (gateways)
//
// =================================================================================================
// Test scheme
// -------------------------------------------------------------------------------------------------
//          +----- b -----+
//          |             |
//  A0----- a ----------- c -----C0
//          |             |
//          +----- d -----+
//

using mesh_network_t = test::meshnet::network<node_pool_t>;

std::atomic_int g_channels_established_counter3 {0};
std::atomic_int g_channels_established_counter6 {0};
std::atomic_int g_expired_counter3 {0};
std::atomic_int g_expired_counter6 {0};
pfs::synchronized<bit_matrix<3>> g_route_matrix3;
pfs::synchronized<bit_matrix<6>> g_route_matrix6;
pfs::synchronized<bit_matrix<3>> g_message_matrix3;
pfs::synchronized<bit_matrix<6>> g_message_matrix6;
std::string g_text3;
std::string g_text6;

#ifdef DOCTEST_VERSION
// See https://github.com/doctest/doctest/issues/345
inline char const * current_doctest_name ()
{
    return doctest::detail::g_cs->currentTest->m_name;
}
#endif

static void sigterm_handler (int sig)
{
    MESSAGE("Force interrupt all nodes by signal: ", sig);
    mesh_network_t::instance()->interrupt_all();
}

static std::string random_text ()
{
    lorem::lorem_ipsum ipsum;
    ipsum.set_paragraph_count(1);
    ipsum.set_sentence_count(10);
    ipsum.set_word_count(20);

    auto para = ipsum();
    std::string text;
    char const * delim = "" ;

    for (auto const & sentence: para[0]) {
        text += delim + sentence;
        delim = "\n";
    }

    return text;
}

#if 1
TEST_CASE("short chain unreachable") {
    LOGD(TAG, "==========================================");
    LOGD(TAG, "= TEST CASE: {}", current_doctest_name());
    LOGD(TAG, "==========================================");

    netty::startup_guard netty_startup;

    mesh_network_t net {"a", "A0", "C0"};

    net.on_channel_established = [] (std::string const & source_name, std::string const & target_name
        , bool /*is_gateway*/)
    {
        LOGD(TAG, "Channel established {:>2} <--> {:>2}", source_name, target_name);
        ++g_channels_established_counter3;
    };

    net.on_node_expired = [] (std::string const & source_name, std::string const & target_name)
    {
        LOGD(TAG, "{}: Node expired: {}", source_name, target_name);
        ++g_expired_counter3;
    };

    net.on_route_ready = [] (std::string const & source_name, std::string const & target_name
        , std::vector<node_id> /*gw_chain*/, std::size_t source_index, std::size_t target_index)
    {
        g_route_matrix3.wlock()->set(source_index, target_index, true);
    };

    net.on_data_received = [] (std::string const & receiver_name, std::string const & sender_name
        , int priority, archive_t bytes, std::size_t source_index, std::size_t target_index)
    {
        LOGD(TAG, "Message received by {} from {}", receiver_name, sender_name);

        std::string text(bytes.data(), bytes.size());

        REQUIRE_EQ(text, g_text3);

        // fmt::println(text);

        g_message_matrix3.wlock()->set(source_index, target_index, true);
    };

    constexpr bool BEHIND_NAT = true;
    g_text3 = random_text();

    net.connect_host("A0", "a", BEHIND_NAT);
    net.connect_host("C0", "a", BEHIND_NAT);

    tools::signal_guard signal_guard {SIGINT, sigterm_handler};

    net.run_all();

    CHECK(tools::wait_atomic_counter(g_channels_established_counter3, 2));
    CHECK(tools::wait_matrix_count(g_route_matrix3, 6));
    CHECK(tools::print_matrix_with_check(*g_route_matrix3.rlock(), {"a", "A0", "C0"}));

    net.print_routing_table("A0");

    net.send_message("A0", "C0", g_text3);

    CHECK(tools::wait_matrix_count(g_message_matrix3, 1));

    net.destroy("C0");

    net.send_message("A0", "C0", g_text3);
    CHECK(tools::wait_atomic_counter(g_expired_counter3, 1));

    net.print_routing_table("A0");

    net.interrupt_all();
    net.join_all();
}
#endif

#if 1
TEST_CASE("long chain unreachable") {
    LOGD(TAG, "==========================================");
    LOGD(TAG, "= TEST CASE: {}", current_doctest_name());
    LOGD(TAG, "==========================================");

    netty::startup_guard netty_startup;

    mesh_network_t net {"a", "b", "c", "d", "A0", "C0"};

    net.on_channel_established = [] (std::string const & source_name, std::string const & target_name
        , bool /*is_gateway*/)
    {
        LOGD(TAG, "Channel established {:>2} <--> {:>2}", source_name, target_name);
        ++g_channels_established_counter6;
    };

    net.on_node_expired = [] (std::string const & source_name, std::string const & target_name)
    {
        LOGD(TAG, "{}: Node expired: {}", source_name, target_name);
        ++g_expired_counter6;
    };

    net.on_route_ready = [] (std::string const & source_name, std::string const & target_name
        , std::vector<node_id> /*gw_chain*/, std::size_t source_index, std::size_t target_index)
    {
        g_route_matrix6.wlock()->set(source_index, target_index, true);
    };

    net.on_data_received = [] (std::string const & receiver_name, std::string const & sender_name
        , int priority, archive_t bytes, std::size_t source_index, std::size_t target_index)
    {
        LOGD(TAG, "Message received by {} from {}", receiver_name, sender_name);

        std::string text(bytes.data(), bytes.size());

        REQUIRE_EQ(text, g_text6);

        // fmt::println(text);

        g_message_matrix6.wlock()->set(source_index, target_index, true);
    };

    constexpr bool BEHIND_NAT = true;
    g_text6 = random_text();

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
    net.connect_host("C0", "c", BEHIND_NAT);

    tools::signal_guard signal_guard {SIGINT, sigterm_handler};

    net.run_all();

    CHECK(tools::wait_atomic_counter(g_channels_established_counter6, 14));
    CHECK(tools::wait_matrix_count(g_route_matrix6, 30));
    CHECK(tools::print_matrix_with_check(*g_route_matrix6.rlock(), {"a", "b", "c", "d", "A0", "C0"}));

    net.print_routing_table("A0");

    net.send_message("A0", "C0", g_text6);

    CHECK(tools::wait_matrix_count(g_message_matrix6, 1));

    net.destroy("C0");

    net.send_message("A0", "C0", g_text6);
    CHECK(tools::wait_atomic_counter(g_expired_counter6, 1));

    net.print_routing_table("A0");

    net.interrupt_all();
    net.join_all();
}
#endif
