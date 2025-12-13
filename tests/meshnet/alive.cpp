////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.11 Initial version.
//      2025.12.10 Refactored with new version of `mesh_network`.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../tools.hpp"
#include "mesh_network.hpp"
#include <pfs/lorem/wait_atomic_counter.hpp>
#include <pfs/lorem/wait_bitmatrix.hpp>
#include <functional>
#include <vector>

// =================================================================================================
// Legend
// -------------------------------------------------------------------------------------------------
// A0, B0 - regular nodes (nodes)
// a, b, c, d - gateway nodes (gateways)
//
// =================================================================================================
// Scheme 2
// -------------------------------------------------------------------------------------------------
//  A0---a---C0
//
// =================================================================================================
// Scheme 2
// -------------------------------------------------------------------------------------------------
//       +---b---+
//       |       |
//  A0---a-------c---C0
//       |       |
//       +---d---+
//

#define ITERATION_COUNT 1;

#define TEST_SCHEME_1_ENABLED 1
#define TEST_SCHEME_2_ENABLED 0

#define START_TEST_MESSAGE MESSAGE("START Test: ", std::string(tools::current_doctest_name()));
#define END_TEST_MESSAGE MESSAGE("END Test: ", std::string(tools::current_doctest_name()));

using namespace std::placeholders;
constexpr bool BEHIND_NAT = true;

void channel_established_callback (lorem::wait_atomic_counter8 & counter
    , node_pool_spec_t const & source, netty::meshnet::node_index_t
    , node_pool_spec_t const & peer, bool)
{
    LOGD(TAG, "Channel established {:>2} <--> {:>2}", source.first, peer.first);
    ++counter;
};

void channel_destroyed_callback (node_pool_spec_t const & source, node_pool_spec_t const & peer)
{
    LOGD(TAG, "{}: Channel destroyed with {}", source.first, peer.first);
};

template <std::size_t N>
void node_alive_callback (lorem::wait_bitmatrix<N> & matrix
    , node_pool_spec_t const & source, node_pool_spec_t const & peer)
{
    LOGD(TAG, "{}: Node alive: {}", source.first, peer.first);
    matrix.set(source.second, peer.second);
};

template <std::size_t N>
void node_unreachable_callback (lorem::wait_bitmatrix<N> & matrix
    , node_pool_spec_t const & source, node_pool_spec_t const & dest)
{
    LOGD(TAG, "{}: Node unreachable: {}", source.first, peer.first);
    matrix.set(source.second, dest.second);
    matrix.set(dest.second, source.second);
};

template <std::size_t N>
void route_ready_callback (lorem::wait_bitmatrix<N> & matrix
    , node_pool_spec_t const & source, node_pool_spec_t const & peer)
{
    matrix.set(source.second, peer.second);
};

template <std::size_t N>
static void set_main_diagonal (lorem::wait_bitmatrix<N> & matrix)
{
    for (std::size_t i = 0; i < N; i++)
        matrix.set(i, i);
}

// N - Number of nodes
// C - number of expected direct links
template <std::size_t N, std::size_t C>
class scheme_tester
{
public:
    scheme_tester (std::initializer_list<std::string> node_names
        , lorem::wait_bitmatrix<N> & unreachable_matrix
        , std::function<void (mesh_network & net)> connect_scenario)
    {
        std::vector<std::string> node_list {node_names};
        mesh_network net {std::move(node_names)};

        lorem::wait_atomic_counter8 channel_established_counter {C * 2};
        lorem::wait_bitmatrix<N> route_matrix;
        lorem::wait_bitmatrix<N> alive_matrix;

        set_main_diagonal(route_matrix);
        set_main_diagonal(alive_matrix);

        net.on_channel_established = std::bind(channel_established_callback
            , std::ref(channel_established_counter)
            , _1, _2, _3, _4);
        net.on_channel_destroyed = channel_destroyed_callback;
        net.on_node_alive = std::bind(node_alive_callback<N>, std::ref(alive_matrix), _1, _2);
        net.on_node_unreachable = std::bind(node_unreachable_callback<N>, std::ref(unreachable_matrix), _1, _2);
        net.on_route_ready = std::bind(route_ready_callback<N>, std::ref(route_matrix), _1, _2);

        net.set_scenario([&] () {
            CHECK(channel_established_counter());

            CHECK(route_matrix());
            tools::print_matrix(route_matrix.value(), node_list);

            CHECK(alive_matrix());
            tools::print_matrix(alive_matrix.value(), node_list);

            net.destroy("C0");
            CHECK(unreachable_matrix());
            tools::print_matrix(unreachable_matrix.value(), node_list);

            net.interrupt_all();
        });

        net.listen_all();
        connect_scenario(net);
        net.run_all();
    }
};

#if TEST_SCHEME_1_ENABLED
TEST_CASE("scheme 1") { // short chain unreachable
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        lorem::wait_bitmatrix<3> expired_matrix {std::chrono::milliseconds {10000}};
        set_main_diagonal(expired_matrix);

        scheme_tester<3, 2> tester({"a", "A0", "C0"}, expired_matrix
            , [] (mesh_network & net)
        {
            net.connect("A0", "a", BEHIND_NAT);
            net.connect("C0", "a", BEHIND_NAT);
        });

        END_TEST_MESSAGE
    }


    // net.on_data_received = [] (std::string const & receiver_name, std::string const & sender_name
    //     , int priority, archive_t bytes, std::size_t source_index, std::size_t target_index)
    // {
    //     LOGD(TAG, "Message received by {} from {}", receiver_name, sender_name);
    //
    //     std::string text(bytes.data(), bytes.size());
    //
    //     REQUIRE_EQ(text, g_text3);
    //
    //     // fmt::println(text);
    //
    //     g_message_matrix3.wlock()->set(source_index, target_index, true);
    // };
    //
    // g_text3 = random_text();
    //
    //
    // net.print_routing_table("A0");
    //
    // net.send_message("A0", "C0", g_text3);
    //
    // CHECK(tools::wait_matrix_count(g_message_matrix3, 1));
    //
    // net.destroy("C0");
    //
    // net.send_message("A0", "C0", g_text3);
    // CHECK(tools::wait_atomic_counter(g_expired_counter3, 1));
    //
    // net.print_routing_table("A0");
    //
    // net.interrupt_all();
    // net.join_all();
}
#endif

#if TEST_SCHEME_2_ENABLED
TEST_CASE("scheme 2") { // long chain unreachable
    // LOGD(TAG, "==========================================");
    // LOGD(TAG, "= TEST CASE: {}", current_doctest_name());
    // LOGD(TAG, "==========================================");

    netty::startup_guard netty_startup;

    mesh_network_t net {"a", "b", "c", "d", "A0", "C0"};

    net.on_channel_established = [] (std::string const & source_name, meshnet_ns::node_index_t
        , std::string const & target_name, bool /*is_gateway*/)
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

    g_text6 = random_text();

    net.listen_all();

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
