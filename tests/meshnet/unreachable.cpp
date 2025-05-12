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
#include "tools.hpp"
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

using namespace netty::patterns;

tools::mesh_network * g_mesh_network_ptr = nullptr;
std::atomic_int g_channels_established_counter {0};
std::atomic_int g_expired_counter {0};
pfs::synchronized<bit_matrix<6>> g_route_matrix;
pfs::synchronized<bit_matrix<6>> g_message_matrix;
std::string g_text;

static void sigterm_handler (int sig)
{
    MESSAGE("Force interrupt: ", sig);

    if (g_mesh_network_ptr != nullptr) {
        MESSAGE("Force interrupt all nodes by signal: ", sig);
        g_mesh_network_ptr->interrupt_all();
    }
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

void tools::mesh_network::on_channel_established (std::string const & source_name
    , node_t::node_id id, bool /*is_gateway*/)
{
    LOGD(TAG, "Channel established {:>2} <--> {:>2}", source_name, node_name_by_id(id));
    g_channels_established_counter++;
}

void tools::mesh_network::on_channel_destroyed (std::string const & /*source_name*/
    , node_t::node_id /*id*/)
{}

void tools::mesh_network::on_duplicated (std::string const &, node_t::node_id
    , std::string const &, netty::socket4_addr)
{};

void tools::mesh_network::on_node_alive (std::string const & /*source_name*/, node_t::node_id /*id*/)
{}

void tools::mesh_network::on_node_expired (std::string const & source_name, node_t::node_id id)
{
    ++g_expired_counter;
}

void tools::mesh_network::on_route_ready (std::string const & source_name, node_t::node_id dest_id
    , std::uint16_t hops)
{
    auto row = serial_number(source_name);
    auto col = serial_number(dest_id);
    g_route_matrix.wlock()->set(row, col, true);
}

void tools::mesh_network::on_message_received (std::string const & receiver_name
    , node_t::node_id sender_id, int priority, std::vector<char> && bytes)
{
    LOGD(TAG, "Message received by {} from {}", receiver_name, node_name_by_id(sender_id));

    std::string text(bytes.data(), bytes.size());

    REQUIRE_EQ(text, g_text);

    // fmt::println(text);

    auto row = serial_number(sender_id);
    auto col = serial_number(receiver_name);
    g_message_matrix.wlock()->set(row, col, true);
}

TEST_CASE("unreachable") {
    netty::startup_guard netty_startup;

    tools::mesh_network mesh_network {"a", "b", "c", "d", "A0", "C0"};

    constexpr bool BEHIND_NAT = true;
    g_text = random_text();

    // Connect gateways
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
    mesh_network.connect_host("C0", "c", BEHIND_NAT);

    g_mesh_network_ptr = & mesh_network;

    tools::signal_guard signal_guard {SIGINT, sigterm_handler};

    mesh_network.run_all();

    CHECK(tools::wait_atomic_counter(g_channels_established_counter, 14));
    CHECK(tools::wait_matrix_count(g_route_matrix, 30));
    CHECK(tools::print_matrix_with_check(*g_route_matrix.rlock(), {"a", "b", "c", "d", "A0", "C0"}));

    mesh_network.print_routing_table("A0");

    mesh_network.send("A0", "C0", g_text);

    CHECK(tools::wait_matrix_count(g_message_matrix, 1));

    mesh_network.destroy("C0");

    mesh_network.send("A0", "C0", g_text);
    CHECK(tools::wait_atomic_counter(g_expired_counter, 1));

    mesh_network.print_routing_table("A0");

    mesh_network.interrupt_all();
    mesh_network.join_all();

    g_mesh_network_ptr = nullptr;
}

