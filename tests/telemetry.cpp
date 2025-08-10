////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "tools.hpp"
#include "pfs/netty/startup.hpp"
#include "pfs/netty/patterns/telemetry/telemetry.hpp"
#include <pfs/log.hpp>
#include <chrono>
#include <thread>

using consumer_t = netty::patterns::telemetry::consumer_t;
using producer_t = netty::patterns::telemetry::producer_t;

constexpr std::uint16_t PORT1 = 4242;
constexpr int CONSUMER_LIMIT = 1;
constexpr int MESSAGE_LIMIT = 100;

std::atomic_int g_accepted_counter {0};
std::atomic_int g_received_counter {0};

class visitor: public netty::patterns::telemetry::visitor
{
public:
    void on (std::string const & key, bool value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, "bool");
        CHECK_EQ(value, true);
    }

    void on (std::string const & key, netty::patterns::telemetry::int8_t value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, "int8");
        CHECK_EQ(value, netty::patterns::telemetry::int8_t{42});
    }

    void on (std::string const & key, netty::patterns::telemetry::int16_t value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, "int16");
        CHECK_EQ(value, netty::patterns::telemetry::int16_t{4242});
    }

    void on (std::string const & key, netty::patterns::telemetry::int32_t value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, "int32");
        CHECK_EQ(value, netty::patterns::telemetry::int32_t{424242});
    }

    void on (std::string const & key, netty::patterns::telemetry::int64_t value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, "int64");
        CHECK_EQ(value, netty::patterns::telemetry::int64_t{42424242});
    }

    void on (std::string const & key, netty::patterns::telemetry::float32_t value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, "float32");
        CHECK_EQ(value, netty::patterns::telemetry::float32_t{3.14159});
    }

    void on (std::string const & key, netty::patterns::telemetry::float64_t value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, "float64");
        CHECK_EQ(value, netty::patterns::telemetry::float64_t{2.71828});
    }

    void on (std::string const & key, netty::patterns::telemetry::string_t const & value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, "hello");
        CHECK_EQ(value, "world");
    }

    void on_error (std::string const & errstr) override
    {
        LOGE("", "{}", errstr);
    }
};

TEST_CASE("main") {
    netty::startup_guard netty_startup;

    visitor vis;

    std::atomic_bool prod1_ready_flag {false};
    producer_t prod1 {netty::socket4_addr{netty::inet4_addr::any_addr_value, PORT1}};
    std::array<consumer_t, CONSUMER_LIMIT> consumers;

    auto prod1_thread = std::thread {[&] () {
        prod1.on_accepted([] (netty::socket4_addr) {
            ++g_accepted_counter;
        });

        prod1_ready_flag.store(true);
        prod1.run();
    }};

    std::array<std::thread, CONSUMER_LIMIT> consumer_threads;

    for (int i = 0; i < CONSUMER_LIMIT; i++) {
        consumers[i].set_visitor(std::make_shared<visitor>());

        consumer_threads[i] = std::thread {[&, i] () {
            CHECK(tools::wait_atomic_bool(prod1_ready_flag));

            REQUIRE(consumers[i].connect(netty::socket4_addr{netty::inet4_addr{127,0,0,1}, PORT1}));

            consumers[i].run();
        }};
    }

    CHECK(tools::wait_atomic_counter(g_accepted_counter, CONSUMER_LIMIT));

    for (int i = 0; i < MESSAGE_LIMIT; i++) {
        prod1.push("bool", true);
        prod1.push("int8", netty::patterns::telemetry::int8_t{42});
        prod1.push("int16", netty::patterns::telemetry::int16_t{4242});
        prod1.push("int32", netty::patterns::telemetry::int32_t{424242});
        prod1.push("int64", netty::patterns::telemetry::int64_t{42424242});
        prod1.push("float32", netty::patterns::telemetry::float32_t{3.14159});
        prod1.push("float64", netty::patterns::telemetry::float64_t{2.71828});
        prod1.push("hello", "world");

        prod1.broadcast();

        // It is necessary that the texts arrive separately
        tools::sleep_ms(10);
    }

    // 8 - number of `on()` methods in the visitor.
    CHECK(tools::wait_atomic_counter(g_received_counter, MESSAGE_LIMIT * 8, std::chrono::seconds{10}));

    for (int i = 0; i < CONSUMER_LIMIT; i++) {
        consumers[i].interrupt();
        consumer_threads[i].join();
    }

    prod1.interrupt();
    prod1_thread.join();
}
