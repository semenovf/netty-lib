////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "tools.hpp"
#include "pfs/netty/startup.hpp"
#include "pfs/netty/patterns/pubsub/suitable_pubsub.hpp"
#include "pfs/netty/traits/vector_archive_traits.hpp"
#include <pfs/synchronized.hpp>
#include <cstdint>
#include <thread>

#if NETTY__QT_ENABLED
#   include <QByteArray>
#   include "pfs/netty/traits/qbytearray_archive_traits.hpp"
#endif

constexpr std::uint16_t PORT1 = 4242;
constexpr int SUBSCRIBER_LIMIT = 10;
constexpr int MESSAGE_LIMIT = 100;

std::atomic_int g_accepted_counter {0};
std::array<std::atomic_int, SUBSCRIBER_LIMIT> g_received_counters;

template <typename Archive>
void tests ()
{
    using publisher_t = netty::patterns::pubsub::suitable_publisher<Archive>;
    using subscriber_t = netty::patterns::pubsub::suitable_subscriber<Archive>;

    netty::startup_guard netty_startup;

    std::atomic_bool pub1_ready_flag {false};
    publisher_t pub1 {netty::socket4_addr{netty::inet4_addr::any_addr_value, PORT1}};
    std::array<subscriber_t, SUBSCRIBER_LIMIT> subs;

    auto pub1_thread = std::thread {[&] () {
        pub1.on_accepted([] (netty::socket4_addr) {
            ++g_accepted_counter;
        });

        pub1_ready_flag.store(true);
        pub1.run();
    }};

    std::array<std::thread, SUBSCRIBER_LIMIT> sub_threads;

    for (int i = 0; i < SUBSCRIBER_LIMIT; i++) {
        g_received_counters[i].store(0);

        sub_threads[i] = std::thread {[&, i] () {
            CHECK(tools::wait_atomic_bool(pub1_ready_flag));

            REQUIRE(subs[i].connect(netty::socket4_addr{netty::inet4_addr{127,0,0,1}, PORT1}));

            subs[i].on_data_ready([i] (Archive data) {
                REQUIRE((data[0] == 'B' && data[1] == 'E'
                    && data[data.size() - 2] == 'E' && data[data.size() - 1] == 'D'));

                g_received_counters[i].fetch_add(1);
            });

            subs[i].run();
        }};
    }

    CHECK(tools::wait_atomic_counter(g_accepted_counter, SUBSCRIBER_LIMIT));

    for (int i = 0; i < MESSAGE_LIMIT; i++) {
        auto text = tools::random_small_text();
        text = "BE" + text + "ED";
        pub1.broadcast(text.data(), text.size());

        // It is necessary that the texts arrive separately
        tools::sleep_ms(10);
    }

    CHECK(tools::wait_atomic_counters(g_received_counters, MESSAGE_LIMIT, std::chrono::seconds{10}));

    for (int i = 0; i < SUBSCRIBER_LIMIT; i++) {
        subs[i].interrupt();
        sub_threads[i].join();
    }

    pub1.interrupt();
    pub1_thread.join();
}

TEST_CASE("all") {
    tests<std::vector<char>>();

#if NETTY__QT_ENABLED
    tests<QByteArray>();
#endif
}
