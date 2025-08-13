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

// using consumer_t = netty::patterns::telemetry::consumer_t;
// using producer_t = netty::patterns::telemetry::producer_t;

constexpr std::uint16_t PORT1 = 4242;
constexpr int CONSUMER_LIMIT = 1;
constexpr int MESSAGE_LIMIT = 100;

std::atomic_int g_accepted_counter {0};
std::atomic_int g_received_counter {0};

/////////////////////////////////////////////////////////////////////////////////////////////////
// visitor
/////////////////////////////////////////////////////////////////////////////////////////////////
class visitor: public netty::patterns::telemetry::visitor<std::string>
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

/////////////////////////////////////////////////////////////////////////////////////////////////
// visitor_u16
/////////////////////////////////////////////////////////////////////////////////////////////////
constexpr std::uint16_t BOOL_KEY = 1;
constexpr std::uint16_t I8_KEY   = 2;
constexpr std::uint16_t I16_KEY  = 3;
constexpr std::uint16_t I32_KEY  = 4;
constexpr std::uint16_t I64_KEY  = 5;
constexpr std::uint16_t F32_KEY  = 6;
constexpr std::uint16_t F64_KEY  = 7;
constexpr std::uint16_t STR_KEY  = 8;

class visitor_u16: public netty::patterns::telemetry::visitor<std::uint16_t>
{
public:
    void on (std::uint16_t const & key, bool value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, BOOL_KEY);
        CHECK_EQ(value, true);
    }

    void on (std::uint16_t const & key, netty::patterns::telemetry::int8_t value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, I8_KEY);
        CHECK_EQ(value, netty::patterns::telemetry::int8_t{42});
    }

    void on (std::uint16_t const & key, netty::patterns::telemetry::int16_t value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, I16_KEY);
        CHECK_EQ(value, netty::patterns::telemetry::int16_t{4242});
    }

    void on (std::uint16_t const & key, netty::patterns::telemetry::int32_t value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, I32_KEY);
        CHECK_EQ(value, netty::patterns::telemetry::int32_t{424242});
    }

    void on (std::uint16_t const & key, netty::patterns::telemetry::int64_t value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, I64_KEY);
        CHECK_EQ(value, netty::patterns::telemetry::int64_t{42424242});
    }

    void on (std::uint16_t const & key, netty::patterns::telemetry::float32_t value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, F32_KEY);
        CHECK_EQ(value, netty::patterns::telemetry::float32_t{3.14159});
    }

    void on (std::uint16_t const & key, netty::patterns::telemetry::float64_t value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, F64_KEY);
        CHECK_EQ(value, netty::patterns::telemetry::float64_t{2.71828});
    }

    void on (std::uint16_t const & key, netty::patterns::telemetry::string_t const & value) override
    {
        ++g_received_counter;
        CHECK_EQ(key, STR_KEY);
        CHECK_EQ(value, "world");
    }

    void on_error (std::string const & errstr) override
    {
        LOGE("", "{}", errstr);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////
// test_body
/////////////////////////////////////////////////////////////////////////////////////////////////
template <typename KeyT>
struct producer_traits;

template <typename KeyT>
struct consumer_traits;

template <>
struct producer_traits<std::string>
{
    using type = netty::patterns::telemetry::producer_t;

    static void push_data_to (type & prod)
    {
        prod.push("bool", true);
        prod.push("int8", netty::patterns::telemetry::int8_t{42});
        prod.push("int16", netty::patterns::telemetry::int16_t{4242});
        prod.push("int32", netty::patterns::telemetry::int32_t{424242});
        prod.push("int64", netty::patterns::telemetry::int64_t{42424242});
        prod.push("float32", netty::patterns::telemetry::float32_t{3.14159});
        prod.push("float64", netty::patterns::telemetry::float64_t{2.71828});
        prod.push("hello", "world");
    }
};

template <>
struct producer_traits<std::uint16_t>
{
    using type = netty::patterns::telemetry::producer_u16_t;

    static void push_data_to (type & prod)
    {
        prod.push(BOOL_KEY, true);
        prod.push(I8_KEY, netty::patterns::telemetry::int8_t{42});
        prod.push(I16_KEY, netty::patterns::telemetry::int16_t{4242});
        prod.push(I32_KEY, netty::patterns::telemetry::int32_t{424242});
        prod.push(I64_KEY, netty::patterns::telemetry::int64_t{42424242});
        prod.push(F32_KEY, netty::patterns::telemetry::float32_t{3.14159});
        prod.push(F64_KEY, netty::patterns::telemetry::float64_t{2.71828});
        prod.push(STR_KEY, "world");
    }
};

template <>
struct consumer_traits<std::string>
{
    using type = netty::patterns::telemetry::consumer_t;
};

template <>
struct consumer_traits<std::uint16_t>
{
    using type = netty::patterns::telemetry::consumer_u16_t;
};

template <typename KeyT, typename Visitor>
void test_body ()
{
    using producer_t = typename producer_traits<KeyT>::type;
    using consumer_t = typename consumer_traits<KeyT>::type;

    netty::startup_guard netty_startup;

    g_accepted_counter.store(0, std::memory_order::memory_order_relaxed);
    g_received_counter.store(0, std::memory_order::memory_order_relaxed);

    std::atomic_bool prod1_ready_flag {false};
    producer_t prod1 {netty::socket4_addr{netty::any_inet4_addr(), PORT1}};
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
        consumers[i].set_visitor(std::make_shared<Visitor>());

        consumer_threads[i] = std::thread {[&, i] () {
            CHECK(tools::wait_atomic_bool(prod1_ready_flag));

            REQUIRE(consumers[i].connect(netty::socket4_addr{netty::inet4_addr{127,0,0,1}, PORT1}));

            consumers[i].run();
        }};
    }

    CHECK(tools::wait_atomic_counter(g_accepted_counter, CONSUMER_LIMIT));

    for (int i = 0; i < MESSAGE_LIMIT; i++) {
        producer_traits<KeyT>::push_data_to(prod1);
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

/////////////////////////////////////////////////////////////////////////////////////////////////
// Tests
/////////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("main") {
    test_body<std::string, visitor>();
    test_body<std::uint16_t, visitor_u16>();
}
