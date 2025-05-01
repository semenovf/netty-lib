////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.16 Initial version.
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
// A0, C0 - regular nodes (nodes)
// a, b, c - gateway nodes (gateways)
//
// =================================================================================================
// Test scheme
// -------------------------------------------------------------------------------------------------
//  A0 ---- a ---- b ---- c ---- C0
//

using namespace netty::patterns;

tools::mesh_network_delivery * g_mesh_network_ptr = nullptr;
std::atomic_int g_channels_established_counter {0};
std::atomic_int g_syn_completed_counter {0};
std::atomic_int g_messaged_dispatched_counter {0};
pfs::synchronized<bit_matrix<5>> g_route_matrix;
// pfs::synchronized<bit_matrix<12>> g_message_matrix;
// std::string g_text;

static void sigterm_handler (int sig)
{
    MESSAGE("Force interrupt: ", sig);

    if (g_mesh_network_ptr != nullptr) {
        MESSAGE("Force interrupt all nodes by signal: ", sig);
        g_mesh_network_ptr->interrupt_all();
    }

    // FIXME REMOVE
    std::terminate();
}

// static std::string random_text ()
// {
//     lorem::lorem_ipsum ipsum;
//     ipsum.set_paragraph_count(1);
//     ipsum.set_sentence_count(10);
//     ipsum.set_word_count(20);
//
//     auto para = ipsum();
//     std::string text;
//     char const * delim = "" ;
//
//     for (auto const & sentence: para[0]) {
//         text += delim + sentence;
//         delim = "\n";
//     }
//
//     return text;
// }

void tools::mesh_network::on_channel_established (std::string const & source_name
    , node_t::node_id_rep id_rep, bool /*is_gateway*/)
{
    g_channels_established_counter++;
}

void tools::mesh_network::on_channel_destroyed (std::string const & /*source_name*/
    , node_t::node_id_rep /*id_rep*/)
{}

void tools::mesh_network::on_duplicated (std::string const &, node_t::node_id_rep
    , std::string const &, netty::socket4_addr)
{};

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

void tools::mesh_network::on_message_received (std::string const & receiver_name
    , node_t::node_id_rep sender_id_rep, int priority, std::vector<char> && bytes)
{
    LOGD(TAG, "Message received by {} from {}", receiver_name, node_name_by_id(sender_id_rep));

    auto pdm = _meshnet_delivery_ptr->delivery_manager(receiver_name);

    pdm->process_packet(node_pool_t::node_id_traits::cast(sender_id_rep), std::move(bytes));

    // std::string text(bytes.data(), bytes.size());
    //
    // REQUIRE_EQ(text, g_text);

    // fmt::println(text);

//     auto row = serial_number(sender_id_rep);
//     auto col = serial_number(receiver_name);
//     g_message_matrix.wlock()->set(row, col, true);
}

TEST_CASE("sync delivery") {
    netty::startup_guard netty_startup;

    tools::mesh_network_delivery mesh_network { "a", "b", "c", "A0", "C0" };

    tools::delivery_manager_t::callback_suite callbacks;

    callbacks.on_receiver_ready = [& mesh_network] (tools::delivery_manager_t::address_type addr) {
        LOGD(TAG, "Receiver ready: {}", mesh_network.node_name_by_id(addr));
        g_syn_completed_counter++;
    };

    callbacks.on_message_received = [& mesh_network] (tools::delivery_manager_t::address_type addr
            , std::vector<char> && msg) {
        LOGD(TAG, "Message received from {}: {} bytes", mesh_network.node_name_by_id(addr)
            , msg.size());
    };

    callbacks.on_message_dispatched = [& mesh_network] (tools::delivery_manager_t::address_type addr
            , tools::delivery_manager_t::message_id msgid) {
        LOGD(TAG, "Message dispatched {}: {}", mesh_network.node_name_by_id(addr)
            , tools::delivery_manager_t::message_id_traits::to_string(msgid));

        g_messaged_dispatched_counter++;
    };

    mesh_network.tie_delivery_manager("A0", tools::delivery_manager_t::callback_suite{callbacks});
    mesh_network.tie_delivery_manager("C0", tools::delivery_manager_t::callback_suite{callbacks});

    constexpr bool BEHIND_NAT = true;
    // g_text = random_text();

    // Connect gateways
    mesh_network.connect_host("a", "b");
    mesh_network.connect_host("b", "a");

    mesh_network.connect_host("b", "c");
    mesh_network.connect_host("c", "b");

    mesh_network.connect_host("A0", "a", BEHIND_NAT);
    mesh_network.connect_host("C0", "c", BEHIND_NAT);

    g_mesh_network_ptr = & mesh_network;

    tools::signal_guard signal_guard {SIGINT, sigterm_handler};

    mesh_network.run_all();

    REQUIRE(tools::wait_atomic_counter(g_channels_established_counter, 8));
    CHECK(tools::wait_matrix_count(g_route_matrix, 20));
    CHECK(tools::print_matrix_with_check(*g_route_matrix.rlock(), {"a", "b", "c", "A0", "C0"}));

    mesh_network.send("A0", "C0", "Hello C0 from A0");
    REQUIRE(tools::wait_atomic_counter(g_syn_completed_counter, 1));

    // tools::sleep(5, "Message sent");
    REQUIRE(tools::wait_atomic_counter(g_messaged_dispatched_counter, 1));

    mesh_network.interrupt_all();
    mesh_network.join_all();

    g_mesh_network_ptr = nullptr;
}
