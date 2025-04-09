////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "tools.hpp"
#include <pfs/emitter.hpp>
#include <pfs/lorem/lorem_ipsum.hpp>
#include <pfs/netty/startup.hpp>

// =================================================================================================
// Legend
// -------------------------------------------------------------------------------------------------
// A0, A1, B0, B1, C0, C1, D0, D1 - regular nodes (nodes)
// a, b, c, d - gateway nodes (gateways)
//
// =================================================================================================
// Test scheme
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

using namespace netty::patterns;

tools::mesh_network * g_mesh_network_ptr = nullptr;
std::atomic_int g_channels_established_counter {0};
pfs::synchronized<bit_matrix<12>> g_route_matrix;

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
    , node_t::node_id_rep id_rep, bool /*is_gateway*/)
{
    LOGD(TAG, "Channel established {:>2} <--> {:>2}", source_name, node_name_by_id(id_rep));
    g_channels_established_counter++;
}

void tools::mesh_network::on_channel_destroyed (std::string const & /*source_name*/
    , node_t::node_id_rep /*id_rep*/)
{}

void tools::mesh_network::on_node_alive (std::string const & /*source_name*/
    , node_t::node_id_rep /*id_rep*/)
{}

void tools::mesh_network::on_node_expired (std::string const & /*source_name*/
    , node_t::node_id_rep /*id_rep*/)
{}

void tools::mesh_network::on_route_ready (std::string const & source_name
    , node_t::node_id_rep dest_id_rep, std::uint16_t hops)
{
    auto row = serial_number(source_name);
    auto col = serial_number(dest_id_rep);
    g_route_matrix.wlock()->set(row, col, true);
}

TEST_CASE("messaging") {

    netty::startup_guard netty_startup;

    tools::mesh_network mesh_network {
        "a", "b", "c", "d", "A0", "A1", "B0", "B1", "C0", "C1", "D0", "D1"
    };

    constexpr bool BEHIND_NAT = true;

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
    REQUIRE(tools::wait_matrix_count(g_route_matrix, 132));
    CHECK(tools::print_matrix_with_check(*g_route_matrix.rlock(), {"a", "b", "c", "d"
        , "A0", "A1", "B0", "B1", "C0", "C1", "D0", "D1"}));

    auto text = random_text();
    mesh_network.send("A0", "B1", text);

    tools::sleep(2);
    mesh_network.interrupt_all();

    mesh_network.join_all();


    g_mesh_network_ptr = nullptr;

    fmt::println(random_text());
}
