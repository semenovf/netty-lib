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
//           b---B0
//           |
//  A0---a---e---c---C0
//           |
//           d---D0
//
// =================================================================================================
// Scheme 3
// -------------------------------------------------------------------------------------------------
//       +---b---+
//       |       |
//  A0---a-------c---C0
//       |       |
//       +---d---+
//

#define ITERATION_COUNT 1;

#define TEST_SCHEME_1_ENABLED 0
#define TEST_SCHEME_2_ENABLED 1

#define START_TEST_MESSAGE MESSAGE("START Test: ", std::string(tools::current_doctest_name()));
#define END_TEST_MESSAGE MESSAGE("END Test: ", std::string(tools::current_doctest_name()));

using namespace std::placeholders;
constexpr bool BEHIND_NAT = true;

void channel_established_callback (lorem::wait_atomic_counter8 & counter
    , node_spec_t const & source, netty::meshnet::peer_index_t
    , node_spec_t const & peer, bool)
{
    LOGD(TAG, "Channel established {:>2} <--> {:>2}", source.first, peer.first);
    ++counter;
};

void channel_destroyed_callback (node_spec_t const & source, node_spec_t const & peer)
{
    LOGD(TAG, "{}: Channel destroyed with {}", source.first, peer.first);
};

template <std::size_t N>
void route_ready_callback (lorem::wait_bitmatrix<N> & matrix
    , node_spec_t const & source, node_spec_t const & peer)
{
    matrix.set(source.second, peer.second);
};

template <std::size_t N>
void node_unreachable_callback (lorem::wait_bitmatrix<N> & matrix
    , node_spec_t const & source, node_spec_t const & dest)
{
    LOGD(TAG, "{}: Node unreachable: {}", source.first, dest.first);
    matrix.set(source.second, dest.second);
};

template <std::size_t N>
static void set_main_diagonal (lorem::wait_bitmatrix<N> & matrix)
{
    for (std::size_t i = 0; i < N; i++)
        matrix.set(i, i);
}

// N - number of nodes
// C - number of expected direct links
template <std::size_t N, std::size_t C>
class scheme_tester
{
public:
    scheme_tester (std::initializer_list<std::string> node_names
        , std::string const & node_to_destroy
        , lorem::wait_bitmatrix<N> & unreachable_matrix
        , std::function<void (mesh_network & net)> connect_scenario)
    {
        std::vector<std::string> node_list {node_names};
        mesh_network net {std::move(node_names)};

        lorem::wait_atomic_counter8 channel_established_counter {C * 2};
        lorem::wait_bitmatrix<N> route_ready_matrix;

        set_main_diagonal(route_ready_matrix);

        net.on_channel_established = std::bind(channel_established_callback
            , std::ref(channel_established_counter)
            , _1, _2, _3, _4);
        net.on_channel_destroyed = channel_destroyed_callback;
        net.on_route_ready = std::bind(route_ready_callback<N>, std::ref(route_ready_matrix), _1, _2);
        net.on_node_unreachable = std::bind(node_unreachable_callback<N>, std::ref(unreachable_matrix), _1, _2);

        net.set_scenario([&] () {
            CHECK(channel_established_counter());

            CHECK(route_ready_matrix());
            tools::print_matrix(route_ready_matrix.value(), node_list);

            net.destroy(node_to_destroy);
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
TEST_CASE("scheme 1") {
    static constexpr std::size_t N = 3;
    static constexpr std::size_t C = 2;

    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        lorem::wait_bitmatrix<N> unreachable_matrix {std::chrono::milliseconds {10000}};
        set_main_diagonal(unreachable_matrix);
        unreachable_matrix.set(0, 1);
        unreachable_matrix.set(1, 0);
        unreachable_matrix.set(2, 0);
        unreachable_matrix.set(2, 1);

        scheme_tester<N, C> tester({"a", "A0", "C0"}, "C0", unreachable_matrix
            , [] (mesh_network & net)
        {
            net.connect("A0", "a", BEHIND_NAT);
            net.connect("C0", "a", BEHIND_NAT);
        });

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_2_ENABLED
TEST_CASE("scheme 2") {
    static constexpr std::size_t N = 9;
    static constexpr std::size_t C = 8;

    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        lorem::wait_bitmatrix<N> unreachable_matrix {std::chrono::milliseconds {10000}};
        set_main_diagonal(unreachable_matrix);
        // unreachable_matrix.set(0, 1);
        // unreachable_matrix.set(1, 0);
        // unreachable_matrix.set(2, 0);
        // unreachable_matrix.set(2, 1);

        scheme_tester<N, C> tester({"a", "b", "c", "d", "e", "A0", "B0", "C0", "D0", }, "e"
            , unreachable_matrix, [] (mesh_network & net)
        {
            // Connect gateways
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
