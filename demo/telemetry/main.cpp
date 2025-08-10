////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `ionik-lib`.
//
// Changelog:
//      2025.07.29 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "collector.hpp"
#include "producer.hpp"
#include "visitor.hpp"
#include <pfs/log.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>

#if TELEMETRY_QT_BACKEND
#   include <QCoreApplication>
#endif

std::atomic_int g_complete_counter {0};
std::mutex g_mutex;
std::condition_variable g_cv;


int main (int argc, char * argv[])
{
#if TELEMETRY_QT_BACKEND
    QCoreApplication app {argc, argv};
#endif

    std::thread producer_thread {[] () {
        producer_t p {netty::socket4_addr{netty::any_inet4_addr(), 5555}};

        // Wait collector connected
        std::this_thread::sleep_for(std::chrono::milliseconds{1000});

        p.push("one", netty::telemetry::int8_t{(std::numeric_limits<std::int8_t>::max)()});
        p.push("two", netty::telemetry::int16_t{(std::numeric_limits<std::int16_t>::max)()});
        p.push("three", netty::telemetry::int32_t{(std::numeric_limits<std::int32_t>::max)()});
        p.push("four", netty::telemetry::int64_t{(std::numeric_limits<std::int64_t>::max)()});
        p.push("five", netty::telemetry::float32_t{3.14159});
        p.push("six", netty::telemetry::float64_t{2.71828});
        p.push("seven", std::string{"Hello"});
        p.push("", std::string{}); // Quit
        p.broadcast();
    }};

    visitor vt;
    collector_t c1 {netty::socket4_addr {netty::inet4_addr{127,0,0,1}, 5555}, vt};
    collector_t c2 {netty::socket4_addr {netty::inet4_addr{127,0,0,1}, 5555}, vt};

    std::thread collector_thread1 {[& c1] () {
        c1.run();

#if TELEMETRY_QT_BACKEND
        if (g_complete_counter.load() >= 2)
            qApp->exit();
#endif
    }};

    std::thread collector_thread2 {[& c2] () {
        c2.run();

#if TELEMETRY_QT_BACKEND
        if (g_complete_counter.load() >= 2)
            qApp->exit();
#endif
    }};

#if TELEMETRY_QT_BACKEND
    app.exec();
#else
    std::unique_lock<std::mutex> locker {g_mutex};
    g_cv.wait(locker, [] () { return g_complete_counter.load() >= 2; });
#endif

    c1.interrupt();
    c2.interrupt();
    producer_thread.join();
    collector_thread1.join();
    collector_thread2.join();

    return EXIT_SUCCESS;
}

