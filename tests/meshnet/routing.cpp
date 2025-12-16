////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.11 Initial version (routing_table.cpp).
//      2025.12.10 Refactored with new version of `mesh_network`.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../tools.hpp"
#include "mesh_network.hpp"
#include <pfs/term.hpp>
#include <pfs/lorem/wait_atomic_counter.hpp>
#include <pfs/lorem/wait_bitmatrix.hpp>
#include <functional>
#include <vector>

// =================================================================================================
// Legend
// -------------------------------------------------------------------------------------------------
// A0, A1, B0, B1, C0, C1, D0, D1 - regular nodes (nodes)
// a, b, c, d - gateway nodes (gateways)
//
// =================================================================================================
// Scheme 1
// -------------------------------------------------------------------------------------------------
//  A0---A1
//
// =================================================================================================
// Scheme 2
// -------------------------------------------------------------------------------------------------
//  A0---a---B0
//
// =================================================================================================
// Scheme 3
// -------------------------------------------------------------------------------------------------
//  A0---+       +---B0
//  |    |---a---|    |
//  A1---+       +---B1
//
// =================================================================================================
// Scheme 4
// -------------------------------------------------------------------------------------------------
//  A0---a---b---B0
//
// =================================================================================================
// Scheme 5
// -------------------------------------------------------------------------------------------------
//  A0---+           +---B0
//  |    |---a---b---|   |
//  A1---+           +---B1
//
// =================================================================================================
// Scheme 6
// -------------------------------------------------------------------------------------------------
//             B0---B1
//              |   |
//              +---+
//                |
//            +---b---+
//   A0---+   |       |   +---C0
//   |    |---a-------c---|    |
//   A1---+   |       |   +---C1
//            +---d---+
//                |
//              +---+
//              |   |
//             D0---D1
//
#define ITERATION_COUNT 5;

#define TEST_SCHEME_1_ENABLED 1
#define TEST_SCHEME_2_ENABLED 1
#define TEST_SCHEME_3_ENABLED 1
#define TEST_SCHEME_4_ENABLED 1
#define TEST_SCHEME_5_ENABLED 1
#define TEST_SCHEME_6_ENABLED 1

#define START_TEST_MESSAGE MESSAGE("START Test: ", std::string(tools::current_doctest_name()));
#define END_TEST_MESSAGE MESSAGE("END Test: ", std::string(tools::current_doctest_name()));

using namespace std::placeholders;
using colorzr_t = pfs::term::colorizer;

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

void node_unreachable_callback (node_pool_spec_t const & source, node_pool_spec_t const & peer)
{
    LOGD(TAG, "{}: Node unreachable: {}", source.first, peer.first);
};

template <std::size_t N>
void route_ready_callback (lorem::wait_bitmatrix<N> & matrix, node_pool_spec_t const & source
    , node_pool_spec_t const & peer, std::vector<node_id> gw_chain)
{
    auto hops = gw_chain.size();

    LOGD(TAG, "{}: {}: {}->{} ({})"
        , source.first
        , colorzr_t{}.green().bright().textr("Route ready")
        , source.first
        , peer.first
        , (hops == 0
            ? colorzr_t{}.green().bright().textr("direct access")
            : colorzr_t{}.green().bright().textr(fmt::format("hops={}", hops))));

    matrix.set(source.second, peer.second);
};

// N - Number of nodes
// C - number of expected direct links
template <std::size_t N, std::size_t C>
class scheme_tester
{
private:
    static void set_main_diagonal (lorem::wait_bitmatrix<N> & matrix)
    {
        for (std::size_t i = 0; i < N; i++)
            matrix.set(i, i);
    }

public:
    scheme_tester (std::initializer_list<std::string> node_names
        , std::function<void (mesh_network & net)> connect_scenario)
    {
        std::vector<std::string> node_list {node_names};
        mesh_network net {std::move(node_names)};

        lorem::wait_atomic_counter8 channel_established_counter {C * 2};
        lorem::wait_bitmatrix<N> route_matrix;
        set_main_diagonal(route_matrix);
        net.on_channel_established = std::bind(channel_established_callback
            , std::ref(channel_established_counter)
            , _1, _2, _3, _4);
        net.on_channel_destroyed = channel_destroyed_callback;
        net.on_node_unreachable = node_unreachable_callback;
        net.on_route_ready = std::bind(route_ready_callback<N>, std::ref(route_matrix), _1, _2, _3);

        net.set_scenario([&] () {
            CHECK(channel_established_counter());
            CHECK(route_matrix());
            tools::print_matrix(route_matrix.value(), node_list);
            net.interrupt_all();
        });

        net.listen_all();
        connect_scenario(net);
        net.run_all();
    }
};

#if TEST_SCHEME_1_ENABLED
TEST_CASE("scheme 1") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        scheme_tester<2, 1> tester({"A0", "A1"}, [] (mesh_network & net)
        {
            net.connect("A0", "A1");
            net.connect("A1", "A0");
        });

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_2_ENABLED
TEST_CASE("scheme 2") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        scheme_tester<3, 2> tester({ "a", "A0", "B0" }, [] (mesh_network & net)
        {
            net.connect("A0", "a", BEHIND_NAT);
            net.connect("B0", "a", BEHIND_NAT);
        });

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_3_ENABLED
TEST_CASE("scheme 3") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        scheme_tester<5, 6> tester({ "a", "A0", "A1", "B0", "B1" }, [] (mesh_network & net)
        {
            net.connect("A0", "a", BEHIND_NAT);
            net.connect("A1", "a", BEHIND_NAT);

            net.connect("B0", "a", BEHIND_NAT);
            net.connect("B1", "a", BEHIND_NAT);

            net.connect("A0", "A1");
            net.connect("A1", "A0");
            net.connect("B0", "B1");
            net.connect("B1", "B0");
        });

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_4_ENABLED
TEST_CASE("scheme 4") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        scheme_tester<4, 3> tester({"a", "b", "A0", "B0"}, [] (mesh_network & net)
        {
            // Connect gateways
            net.connect("a", "b");
            net.connect("b", "a");

            net.connect("A0", "a", BEHIND_NAT);
            net.connect("B0", "b", BEHIND_NAT);
        });

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_5_ENABLED
TEST_CASE("scheme 5") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        scheme_tester<6, 7> tester({"a", "b", "A0", "A1", "B0", "B1"}, [] (mesh_network & net)
        {
            // Connect gateways
            net.connect("a", "b");
            net.connect("b", "a");

            net.connect("A0", "a", BEHIND_NAT);
            net.connect("A1", "a", BEHIND_NAT);

            net.connect("B0", "b", BEHIND_NAT);
            net.connect("B1", "b", BEHIND_NAT);

            net.connect("A0", "A1");
            net.connect("A1", "A0");
            net.connect("B0", "B1");
            net.connect("B1", "B0");
        });

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_6_ENABLED
TEST_CASE("scheme 6") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        scheme_tester<12, 17> tester({"a", "b", "c", "d", "A0", "A1", "B0", "B1", "C0", "C1", "D0", "D1"}
            , [] (mesh_network & net)
        {
            net.connect("a", "b");
            net.connect("a", "c");
            net.connect("a", "d");

            net.connect("b", "a");
            net.connect("b", "c");

            net.connect("c", "a");
            net.connect("c", "b");
            net.connect("c", "d");

            net.connect("d", "a");
            net.connect("d", "c");

            net.connect("A0", "a", BEHIND_NAT);
            net.connect("A1", "a", BEHIND_NAT);

            net.connect("B0", "b", BEHIND_NAT);
            net.connect("B1", "b", BEHIND_NAT);

            net.connect("C0", "c", BEHIND_NAT);
            net.connect("C1", "c", BEHIND_NAT);

            net.connect("D0", "d", BEHIND_NAT);
            net.connect("D1", "d", BEHIND_NAT);

            net.connect("A0", "A1");
            net.connect("A1", "A0");
            net.connect("B0", "B1");
            net.connect("B1", "B0");
            net.connect("C0", "C1");
            net.connect("C1", "C0");
            net.connect("D0", "D1");
            net.connect("D1", "D0");
        });

        END_TEST_MESSAGE
    }
}
#endif
