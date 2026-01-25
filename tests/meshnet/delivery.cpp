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
#include "../doctest.h"
#include "../tools.hpp"
#include "mesh_network.hpp"
#include <pfs/crc32.hpp>
#include <pfs/lorem/utils.hpp>
#include <pfs/lorem/wait_atomic_bool.hpp>
#include <pfs/lorem/wait_atomic_counter.hpp>
#include <pfs/lorem/wait_bitmatrix.hpp>
#include <functional>
#include <numeric>

// =================================================================================================
// Legend
// -------------------------------------------------------------------------------------------------
// A0, B0 - regular nodes (nodes)
// a, b - gateway nodes (gateways)
//
// =================================================================================================
// Scheme 1
// -------------------------------------------------------------------------------------------------
// A0---a---b---B0
//
// Option 1: A0 destroyed
// Option 2: B0 destroyed
// Option 3: A0 and B0 destroyed
// Option 4: a destroyed
//

#define ITERATION_COUNT 1; // FIXME 5;

using namespace std::placeholders;

constexpr bool BEHIND_NAT = true;

constexpr int STAGE_INITIAL = 0;
constexpr int STAGE_WAIT_B0_RECEIVER_READY = 1;
static std::atomic_int s_stage {0};
lorem::wait_atomic_bool s_receiver_B0_ready_flag;

inline int percents (std::size_t current, std::size_t total)
{
    return static_cast<int>(static_cast<float>(current)/total * 100);
}

template <std::size_t N>
void route_ready_cb (lorem::wait_bitmatrix<N> & matrix, node_spec_t const & source
    , node_spec_t const & peer, std::size_t /*route_index*/)
{
    matrix.set(source.second, peer.second);
};

void receiver_ready_cb (lorem::wait_atomic_counter8 & counter, node_spec_t const & source
    , node_spec_t const & receiver)
{
    if (s_stage.load() == STAGE_WAIT_B0_RECEIVER_READY) {
        if (source.first == "A0" && receiver.first == "B0")
            s_receiver_B0_ready_flag.set();
    } else {
        ++counter;
    }
}

void message_begin_cb (lorem::wait_atomic_counter32 & counter
    , node_spec_t const & receiver, node_spec_t const & sender
    , std::string const & msgid, std::size_t)
{
    ++counter;
}

void message_received_cb (lorem::wait_atomic_counter32 & counter, std::int32_t msg_crc32_sample
    , node_spec_t const & /*receiver*/, node_spec_t const & /*sender*/, int /*priority*/, archive_t bytes)
{
    LOGD(TAG, "Message received");

    std::int32_t msg_crc32 = pfs::crc32_of_ptr(bytes.data(), bytes.size());
    CHECK_EQ(msg_crc32, msg_crc32_sample);

    ++counter;
}

void message_progress_cb (lorem::wait_atomic_bool & flag, node_spec_t const & receiver, node_spec_t const & sender
    , std::string const & msgid, std::size_t received_size, std::size_t total_size)
{
    auto rate = percents(received_size, total_size);

    if (!flag.value()) {
        flag.set();
        auto pnet = mesh_network::instance();
        pnet->destroy("A0");
    }
}

void message_delivered_cb (lorem::wait_atomic_counter32 & counter, node_spec_t const & /*source*/
    , node_spec_t const & /*receiver*/, std::string const &)
{
    ++counter;
}

// N - number of nodes
template <std::size_t N>
class scheme_tester
{
private:
    static std::string generate_message ()
    {
        std::string result;
        result.resize(10 * 1024 * 1024);
        std::iota(result.begin(), result.end(), 0);
        return result;

        // static constexpr std::size_t MESSAGE_SIZE = 65536 * 5;
        // return lorem::random_binary_data(MESSAGE_SIZE);
    }

public:
    scheme_tester (std::function<void (mesh_network & net)> connect_scenario_cb)
    {
        auto pnet = mesh_network::instance();

        auto msg = generate_message();
        std::int32_t msg_crc32 = pfs::crc32_of(msg);

        lorem::wait_atomic_counter8 receiver_ready_counter {1};
        lorem::wait_atomic_counter32 message_begin_counter {1};
        lorem::wait_atomic_counter32 message_received_counter {1};
        lorem::wait_atomic_counter32 message_delivered_counter {1, std::chrono::seconds{15}};

        lorem::wait_bitmatrix<N> route_matrix;
        pnet->set_main_diagonal(route_matrix);

        lorem::wait_atomic_bool destroy_flag;

        pnet->on_route_ready = std::bind(route_ready_cb<N>, std::ref(route_matrix), _1, _2, _3);
        pnet->on_receiver_ready = std::bind(receiver_ready_cb, std::ref(receiver_ready_counter), _1, _2);
        pnet->on_message_begin = std::bind(message_begin_cb, std::ref(message_begin_counter), _1, _2, _3, _4);
        pnet->on_message_received = std::bind(message_received_cb, std::ref(message_received_counter)
            , msg_crc32, _1, _2, _4, _5);
        pnet->on_message_progress = std::bind(message_progress_cb, std::ref(destroy_flag)
            , _1, _2, _3, _4, _5);
        pnet->on_message_delivered = std::bind(message_delivered_cb, std::ref(message_delivered_counter)
            , _1, _2, _3);

        pnet->set_scenario([&] () {
            REQUIRE(route_matrix.wait());
            REQUIRE_MESSAGE(pnet->send_message("A0", "B0", msg), "A0->B0: route unreachable");
            REQUIRE(receiver_ready_counter.wait());
            REQUIRE(message_begin_counter.wait());

            REQUIRE(destroy_flag.wait());

            // Option 1
            REQUIRE(pnet->launch("A0"));
            pnet->connect("A0", "a", BEHIND_NAT);
            ++s_stage;
            REQUIRE(s_receiver_B0_ready_flag.wait());
            REQUIRE_MESSAGE(pnet->send_message("A0", "B0", msg), "A0->B0: route unreachable");

            REQUIRE(message_received_counter.wait());
            REQUIRE(message_delivered_counter.wait());
            pnet->interrupt_all();
        });

        pnet->listen_all();
        connect_scenario_cb(*pnet);
        pnet->run_all();
    }
};

TEST_CASE("scheme 1") {
    static constexpr std::size_t N = 4;

    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        mesh_network net {"a", "b", "A0", "B0"};

        scheme_tester<N> tester([] (mesh_network & net)
        {
            net.connect("a", "b");
            net.connect("b", "a");

            net.connect("A0", "a", BEHIND_NAT);
            net.connect("B0", "b", BEHIND_NAT);
        });

        END_TEST_MESSAGE
    }
}
