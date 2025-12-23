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
#include <functional>
#include <vector>

// =================================================================================================
// Legend
// -------------------------------------------------------------------------------------------------
// A0, B0, C0, D0 - regular nodes (nodes)
// a, b, c, d, e - gateway nodes (gateways)
//
// =================================================================================================
// Scheme 1
// -------------------------------------------------------------------------------------------------
//  A0---a---B0
//
// =================================================================================================
// Scheme 2
// -------------------------------------------------------------------------------------------------
//  A0---a---e---b---B0
//
// =================================================================================================
// Scheme 3
// -------------------------------------------------------------------------------------------------
//           b---B0
//           |
//  A0---a---e---c---C0
//           |
//           d---D0
//
// =================================================================================================
// Scheme 4
// -------------------------------------------------------------------------------------------------
//       +---b---+
//       |       |
//  A0---a---e---c---C0
//       |       |
//       +---d---+
//

#define ITERATION_COUNT 5;

#define TEST_SCHEME_1_ENABLED 1
#define TEST_SCHEME_2_ENABLED 1
#define TEST_SCHEME_3_ENABLED 1
#define TEST_SCHEME_4_ENABLED 1

#define START_TEST_MESSAGE MESSAGE("START Test: ", std::string(tools::current_doctest_name()));
#define END_TEST_MESSAGE MESSAGE("END Test: ", std::string(tools::current_doctest_name()));

using namespace std::placeholders;
constexpr bool BEHIND_NAT = true;
constexpr std::chrono::seconds BITMATRIX_TIME_LIMIT {2};

void channel_established_cb (lorem::wait_atomic_counter8 & counter
    , node_spec_t const & source, netty::meshnet::peer_index_t
    , node_spec_t const & peer, bool)
{
    LOGD(TAG, "Channel established {:>2} <--> {:>2}", source.first, peer.first);
    ++counter;
};

void channel_destroyed_cb (node_spec_t const & source, node_spec_t const & peer)
{
    LOGD(TAG, "{}: Channel destroyed with {}", source.first, peer.first);
};

template <std::size_t N>
void route_ready_cb (lorem::wait_bitmatrix<N> & matrix, node_spec_t const & source
    , node_spec_t const & peer)
{
    LOGD(TAG, "{}: Route ready to: {}", source.first, peer.first);
    matrix.set(source.second, peer.second);
};

template <std::size_t N>
void node_unreachable_cb (lorem::wait_bitmatrix<N> & matrix
    , node_spec_t const & source, node_spec_t const & dest)
{
    LOGD(TAG, "{}: Node unreachable: {}", source.first, dest.first);
    matrix.set(source.second, dest.second, false);
};

// N - number of nodes
// C - number of expected direct links
template <std::size_t N, std::size_t C>
class scheme_tester
{
public:
    scheme_tester (std::string const & node_to_destroy
        , lorem::wait_bitmatrix<N> const & unreachable_matrix_sample
        , std::function<void (mesh_network & net)> connect_scenario_cb)
    {
        auto pnet = mesh_network::instance();

        lorem::wait_atomic_counter8 channel_established_counter {C * 2};
        lorem::wait_bitmatrix<N> route_ready_matrix;
        lorem::wait_bitmatrix<N> unreachable_matrix;

        pnet->set_main_diagonal(route_ready_matrix);
        pnet->set_all(unreachable_matrix);
        pnet->set_main_diagonal(unreachable_matrix, false);
        pnet->set_row(unreachable_matrix, node_to_destroy, false);

        pnet->on_channel_established = std::bind(channel_established_cb
            , std::ref(channel_established_counter)
            , _1, _2, _3, _4);
        pnet->on_channel_destroyed = channel_destroyed_cb;
        pnet->on_route_ready = std::bind(route_ready_cb<N>, std::ref(route_ready_matrix), _1, _2);
        pnet->on_node_unreachable = std::bind(node_unreachable_cb<N>
            , std::ref(unreachable_matrix), _1, _2);

        pnet->set_scenario([&] () {
            CHECK(channel_established_counter());

            CHECK(route_ready_matrix());
            tools::print_matrix(route_ready_matrix.value(), pnet->node_names());

            pnet->destroy(node_to_destroy);

            CHECK(unreachable_matrix.wait_eq(unreachable_matrix_sample));

            tools::print_matrix(unreachable_matrix.value(), pnet->node_names());
            tools::print_matrix(unreachable_matrix_sample.value(), pnet->node_names());

            pnet->interrupt_all();
        });

        pnet->listen_all();
        connect_scenario_cb(*pnet);
        pnet->run_all();
    }
};

#if TEST_SCHEME_1_ENABLED
TEST_CASE("scheme 1") {
    static constexpr std::size_t N = 3;
    static constexpr std::size_t C = 2;

    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        mesh_network net {"a", "A0", "B0"};

        lorem::wait_bitmatrix<N> unreachable_matrix {BITMATRIX_TIME_LIMIT};
        net.set_all(unreachable_matrix);
        net.set_main_diagonal(unreachable_matrix, false);
        net.set_row(unreachable_matrix, "B0", false);
        net.set(unreachable_matrix, "a", "B0", false);
        net.set(unreachable_matrix, "A0", "B0", false);

        scheme_tester<N, C> tester("B0", unreachable_matrix
            , [] (mesh_network & net)
            {
                net.connect("A0", "a", BEHIND_NAT);
                net.connect("B0", "a", BEHIND_NAT);
            });

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_2_ENABLED
TEST_CASE("scheme 2") {
    static constexpr std::size_t N = 5;
    static constexpr std::size_t C = 4;

    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        mesh_network net {"a", "e", "b", "A0", "B0"};

        lorem::wait_bitmatrix<N> unreachable_matrix {BITMATRIX_TIME_LIMIT};
        net.set_all(unreachable_matrix);
        net.set_main_diagonal(unreachable_matrix, false);
        net.set_row(unreachable_matrix, "e", false);
        net.set(unreachable_matrix, "a", "e", false);
        net.set(unreachable_matrix, "a", "b", false);
        net.set(unreachable_matrix, "a", "B0", false);
        net.set(unreachable_matrix, "A0", "e", false);
        net.set(unreachable_matrix, "A0", "b", false);
        net.set(unreachable_matrix, "A0", "B0", false);
        net.set(unreachable_matrix, "b", "e", false);
        net.set(unreachable_matrix, "b", "a", false);
        net.set(unreachable_matrix, "b", "A0", false);
        net.set(unreachable_matrix, "B0", "e", false);
        net.set(unreachable_matrix, "B0", "a", false);
        net.set(unreachable_matrix, "B0", "A0", false);

        scheme_tester<N, C> tester("e", unreachable_matrix, [] (mesh_network & net)
        {
            net.connect("a", "e");
            net.connect("e", "a");
            net.connect("b", "e");
            net.connect("e", "b");

            net.connect("A0", "a", BEHIND_NAT);
            net.connect("B0", "b", BEHIND_NAT);
        });

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_3_ENABLED
TEST_CASE("scheme 3") {
    static constexpr std::size_t N = 9;
    static constexpr std::size_t C = 8;

    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        mesh_network net {"a", "b", "c", "d", "e", "A0", "B0", "C0", "D0"};

        lorem::wait_bitmatrix<N> unreachable_matrix {BITMATRIX_TIME_LIMIT};

        net.set(unreachable_matrix, "a", "A0");
        net.set(unreachable_matrix, "A0", "a");
        net.set(unreachable_matrix, "b", "B0");
        net.set(unreachable_matrix, "B0", "b");
        net.set(unreachable_matrix, "c", "C0");
        net.set(unreachable_matrix, "C0", "c");
        net.set(unreachable_matrix, "d", "D0");
        net.set(unreachable_matrix, "D0", "d");

        scheme_tester<N, C> tester("e", unreachable_matrix, [] (mesh_network & net)
        {
            net.connect("a", "e");
            net.connect("b", "e");
            net.connect("c", "e");
            net.connect("d", "e");
            net.connect("e", "a");
            net.connect("e", "b");
            net.connect("e", "c");
            net.connect("e", "d");

            net.connect("A0", "a", BEHIND_NAT);
            net.connect("B0", "b", BEHIND_NAT);
            net.connect("C0", "c", BEHIND_NAT);
            net.connect("D0", "d", BEHIND_NAT);
        });

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_4_ENABLED
TEST_CASE("scheme 4") {
    static constexpr std::size_t N = 7;
    static constexpr std::size_t C = 8;

    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        mesh_network net {"a", "b", "c", "d", "e", "A0", "C0"};

        lorem::wait_bitmatrix<N> unreachable_matrix {BITMATRIX_TIME_LIMIT};
        net.set_all(unreachable_matrix);
        net.set_main_diagonal(unreachable_matrix, false);
        net.set_row(unreachable_matrix, "e", false);
        net.set(unreachable_matrix, "a", "e", false);
        net.set(unreachable_matrix, "b", "e", false);
        net.set(unreachable_matrix, "c", "e", false);
        net.set(unreachable_matrix, "d", "e", false);
        net.set(unreachable_matrix, "A0", "e", false);
        net.set(unreachable_matrix, "C0", "e", false);

        scheme_tester<N, C> tester("e", unreachable_matrix, [] (mesh_network & net)
        {
            net.connect("a", "b");
            net.connect("a", "d");
            net.connect("a", "e");
            net.connect("b", "a");
            net.connect("b", "c");
            net.connect("c", "b");
            net.connect("c", "d");
            net.connect("c", "e");
            net.connect("d", "a");
            net.connect("d", "c");
            net.connect("e", "a");
            net.connect("e", "c");

            net.connect("A0", "a", BEHIND_NAT);
            net.connect("C0", "c", BEHIND_NAT);
        });

        END_TEST_MESSAGE
    }
}
#endif
