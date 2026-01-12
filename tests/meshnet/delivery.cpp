////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.08 Initial version.
//      2025.12.24 Refactored and fixed with new version of `mesh_network`.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define NETTY__TESTS_USE_MESHNET_NODE_POOL_RD
#include "../doctest.h"
#include "../tools.hpp"
#include "mesh_network.hpp"
#include <pfs/lorem/wait_atomic_counter.hpp>
#include <pfs/lorem/wait_bitmatrix.hpp>
#include <functional>

// =================================================================================================
// Legend
// -------------------------------------------------------------------------------------------------
// A0, A1, B0, B1, C0, C1, D0, D1 - regular nodes (nodes)
// a, b, c, d, e - gateway nodes (gateways)
//
// =================================================================================================
// Scheme 1
// -------------------------------------------------------------------------------------------------
//      +---c---+
//      |       |
// A0---a---e---b---B0
//      |       |
//      +---d---+
//

#define ITERATION_COUNT 5;

using namespace std::placeholders;

constexpr bool BEHIND_NAT = true;

void channel_established_cb (lorem::wait_atomic_counter8 & counter
    , node_spec_t const & source, netty::meshnet::peer_index_t
    , node_spec_t const & peer, bool)
{
    LOGD(TAG, "Channel established {:>2} <--> {:>2}", source.first, peer.first);

    // Here can be set frame size
    // std::uint16_t frame_size = 100;
    // mesh_network_t::instance()->set_frame_size(source_name, source_index, peer_name, frame_size);

    ++counter;
};

template <std::size_t N>
void route_ready_cb (lorem::wait_bitmatrix<N> & matrix, node_spec_t const & source
    , node_spec_t const & peer, std::size_t /*route_index*/)
{
    matrix.set(source.second, peer.second);
};

TEST_CASE("delivery") {
    static constexpr std::size_t N = 7;

    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        mesh_network net {"a", "b", "c", "d", "e", "A0", "B0"};

        lorem::wait_atomic_counter8 channel_established_counter {static_cast<std::uint8_t>(16)};
        lorem::wait_bitmatrix<N> route_matrix;
        net.set_main_diagonal(route_matrix);

        net.on_channel_established = std::bind(channel_established_cb
            , std::ref(channel_established_counter)
            , _1, _2, _3, _4);

        net.on_route_ready = std::bind(route_ready_cb<N>, std::ref(route_matrix), _1, _2, _3);

        net.set_scenario([&] {
            REQUIRE(channel_established_counter.wait());
            REQUIRE(route_matrix.wait());
            net.interrupt_all();
        });

        // Run before connect calls
        net.listen_all();

        // Connect gateways
        net.connect("a", "c");
        net.connect("a", "d");
        net.connect("a", "e");
        net.connect("b", "c");
        net.connect("b", "d");
        net.connect("b", "e");
        net.connect("c", "a");
        net.connect("c", "b");
        net.connect("d", "a");
        net.connect("d", "b");
        net.connect("e", "a");
        net.connect("e", "b");

        net.connect("A0", "a", BEHIND_NAT);
        net.connect("B0", "b", BEHIND_NAT);

        net.run_all();

        END_TEST_MESSAGE
    }
}
