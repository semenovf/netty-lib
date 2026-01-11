////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.16 Initial version.
//      2025.12.23 Refactored and fixed with new version of `mesh_network`.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../tools.hpp"
#include "mesh_network.hpp"
#include <pfs/term.hpp>
#include <pfs/lorem/utils.hpp>
#include <pfs/lorem/wait_atomic_counter.hpp>
#include <pfs/lorem/wait_bitmatrix.hpp>
#include <functional>
#include <string>
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
//  A0---A1
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

#define ITERATION_COUNT 10;

#define TEST_SCHEME_1_ENABLED 0
#define TEST_SCHEME_2_ENABLED 0
#define TEST_SCHEME_3_ENABLED 1
#define TEST_SCHEME_4_ENABLED 0

#define START_TEST_MESSAGE MESSAGE("START Test: ", std::string(tools::current_doctest_name()));
#define END_TEST_MESSAGE MESSAGE("END Test: ", std::string(tools::current_doctest_name()));

using namespace std::placeholders;
using colorzr_t = pfs::term::colorizer;

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

void channel_destroyed_cb (node_spec_t const & source, node_spec_t const & peer)
{
    LOGD(TAG, "{}: Channel destroyed with {}", source.first, peer.first);
};

template <std::size_t N>
void route_ready_cb (lorem::wait_bitmatrix<N> & matrix, node_spec_t const & source
    , node_spec_t const & peer, std::size_t /*route_index*/)
{
    LOGD(TAG, "{}: {}: {}-->{}"
        , source.first
        , colorzr_t{}.green().bright().textr("Route ready")
        , source.first
        , peer.first);

    matrix.set(source.second, peer.second);
};

void data_received_cb (lorem::wait_atomic_counter32 & counter, node_spec_t const & receiver
    , node_spec_t const & sender, int priority, archive_t bytes)
{
    LOGD(TAG, "{}: Data received: {}-->{} ({} bytes)", receiver.first, sender.first
        , receiver.first, bytes.size());
    ++counter;
}

#ifdef NETTY__TESTS_USE_MESHNET_RELIABLE_NODE
void receiver_ready_cb (lorem::wait_atomic_counter8 & counter, node_spec_t const & source
    , node_spec_t const & receiver)
{
    LOGD(TAG, "{}: Receiver ready: {}", source.first, receiver.first);
    ++counter;
}

void message_delivered_cb (lorem::wait_atomic_counter32 & counter, node_spec_t const & /*source*/
    , node_spec_t const & /*receiver*/, std::string const &)
{
    ++counter;
}

void message_receiving_begin_cb (lorem::wait_atomic_counter32 & counter
    , node_spec_t const & receiver, node_spec_t const & sender
    , std::string const & msgid, std::size_t)
{
    ++counter;
}

// Unusable, only for API check.
void message_receiving_progress_cb (node_spec_t const & receiver, node_spec_t const & sender
    , std::string const & msgid, std::size_t received_size, std::size_t total_size)
{}

void report_received_cb (lorem::wait_atomic_counter32 & counter, node_spec_t const & /*receiver*/
    , node_spec_t const & /*sender*/, int /*priority*/, archive_t /*bytes*/)
{
    ++counter;
}

#endif

// N - number of nodes
// C - number of expected direct links
template <std::size_t N, std::size_t C>
class scheme_tester
{
    static std::vector<std::string> generate_messages ()
    {
        std::vector<std::string> result;

        for (std::size_t i = 1; i <= 65536; i *= 2)
            result.push_back(lorem::random_binary_data(i));

        return result;
    }

public:
    scheme_tester (std::function<void (mesh_network & net)> connect_scenario_cb)
    {
        auto pnet = mesh_network::instance();

        auto messages = generate_messages();

        std::uint8_t expected_channels_established = C * 2;
        std::uint32_t expected_messages_received = (N * N - N) * messages.size();
        std::uint32_t expected_reports_received = expected_messages_received;

        LOGD(TAG, "Expected channels established: {}", expected_channels_established);
        LOGD(TAG, "Expected messages received: {}", expected_messages_received);
        LOGD(TAG, "Expected reports received: {}", expected_reports_received);

        lorem::wait_atomic_counter8 channel_established_counter {expected_channels_established};
        lorem::wait_atomic_counter32 message_received_counter {expected_messages_received};

        lorem::wait_bitmatrix<N> route_matrix;
        pnet->set_main_diagonal(route_matrix);

        pnet->on_channel_established = std::bind(channel_established_cb
            , std::ref(channel_established_counter), _1, _2, _3, _4);
        pnet->on_channel_destroyed = channel_destroyed_cb;
        pnet->on_route_ready = std::bind(route_ready_cb<N>, std::ref(route_matrix), _1, _2, _3);

#ifdef NETTY__TESTS_USE_MESHNET_RELIABLE_NODE
        lorem::wait_atomic_counter8 receiver_ready_counter {N * N - N};
        lorem::wait_atomic_counter32 message_delivered_counter {expected_messages_received};
        lorem::wait_atomic_counter32 message_receiving_begin_counter {expected_messages_received};
        lorem::wait_atomic_counter32 report_received_counter {expected_reports_received};

        pnet->on_receiver_ready = std::bind(receiver_ready_cb, std::ref(receiver_ready_counter), _1, _2);
        pnet->on_message_delivered = std::bind(message_delivered_cb, std::ref(message_delivered_counter)
            , _1, _2, _3);
        pnet->on_message_received = std::bind(data_received_cb, std::ref(message_received_counter)
            , _1, _2, _4, _5);
        pnet->on_message_start_receiving = std::bind(message_receiving_begin_cb
            , std::ref(message_receiving_begin_counter), _1, _2, _3, _4);
        pnet->on_message_receiving_progress = message_receiving_progress_cb;

        pnet->on_report_received = std::bind(report_received_cb, std::ref(report_received_counter)
            , _1, _2, _3, _4);
#else
        pnet->on_data_received = std::bind(data_received_cb, std::ref(message_received_counter)
            , _1, _2, _3, _4);
#endif

        pnet->set_scenario([&] () {
            REQUIRE(channel_established_counter.wait());
            REQUIRE(route_matrix.wait());

            auto const & node_names = pnet->node_names();
            auto routes = pnet->shuffle_messages(node_names, node_names, messages);

            for (auto const & x: routes) {
                pnet->send_message(std::get<0>(x), std::get<1>(x), std::get<2>(x));

#ifdef NETTY__TESTS_USE_MESHNET_RELIABLE_NODE
                pnet->send_report(std::get<0>(x), std::get<1>(x), std::get<2>(x));
#endif
            }

#ifdef NETTY__TESTS_USE_MESHNET_RELIABLE_NODE
            REQUIRE(receiver_ready_counter.wait());
            REQUIRE(message_receiving_begin_counter.wait());
#endif
            REQUIRE(message_received_counter.wait());
#ifdef NETTY__TESTS_USE_MESHNET_RELIABLE_NODE
            REQUIRE(report_received_counter.wait());
            REQUIRE(message_delivered_counter.wait());
#endif
            pnet->interrupt_all();
        });

        pnet->listen_all();
        connect_scenario_cb(*pnet);
        pnet->run_all();
    }
};

#if TEST_SCHEME_1_ENABLED
TEST_CASE("scheme 1") {
    static constexpr std::size_t N = 2;
    static constexpr std::size_t C = 1;

    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        mesh_network net {"A0", "A1"};

        scheme_tester<N, C> tester([] (mesh_network & net)
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
    static constexpr std::size_t N = 5;
    static constexpr std::size_t C = 4;

    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        mesh_network net {"a", "b", "e", "A0", "B0"};

        scheme_tester<N, C> tester([] (mesh_network & net)
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

        scheme_tester<N, C> tester([] (mesh_network & net)
            {
                net.connect("a", "e");
                net.connect("e", "a");
                net.connect("b", "e");
                net.connect("e", "b");
                net.connect("c", "e");
                net.connect("e", "c");
                net.connect("d", "e");
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

        scheme_tester<N, C> tester([] (mesh_network & net)
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
