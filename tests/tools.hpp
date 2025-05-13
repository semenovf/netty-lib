////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.11 Initial version.
//      2025.04.07 Moved to tools.hpp.
////////////////////////////////////////////////////////////////////////////////
#include "bit_matrix.hpp"
#include <pfs/countdown_timer.hpp>
#include <pfs/fmt.hpp>
#include <pfs/log.hpp>
#include <pfs/lorem/lorem_ipsum.hpp>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <thread>
#include <signal.h>

namespace tools {

inline void sleep (int timeout, std::string const & description = std::string{})
{
    if (description.empty()) {
        LOGD("", "Waiting for {} seconds", timeout);
    } else {
        LOGD("", "{}: waiting for {} seconds", description, timeout);
    }

    std::this_thread::sleep_for(std::chrono::seconds{timeout});
}

template <typename AtomicCounter>
bool wait_atomic_counter (AtomicCounter & counter
    , typename AtomicCounter::value_type limit
    , std::chrono::milliseconds timelimit = std::chrono::milliseconds{5000})
{
    pfs::countdown_timer<std::milli> timer {timelimit};

    while (counter.load() < limit && timer.remain_count() > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

    return !(counter.load() < limit);
}

template <typename SyncRouteMatrix>
bool wait_matrix_count (SyncRouteMatrix & safe_matrix, std::size_t limit
    , std::chrono::milliseconds timelimit = std::chrono::milliseconds{5000})
{
    pfs::countdown_timer<std::milli> timer {timelimit};

    while (safe_matrix.rlock()->count() < limit && timer.remain_count() > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

    return !(safe_matrix.rlock()->count() < limit);
}

#if _MSC_VER
using sighandler_t = void (*) (int);
#endif

class signal_guard
{
    int sig {0};
    sighandler_t old_handler;

public:
    signal_guard (int sig, sighandler_t handler)
        : sig(sig)
    {
        old_handler = signal(sig, handler);
    }

    ~signal_guard ()
    {
        signal(sig, old_handler);
    }
};

template <typename RouteMatrix>
bool print_matrix_with_check (RouteMatrix & m, std::vector<char const *> caption)
{
    bool success = true;
    fmt::print("[   ]");

    for (std::size_t j = 0; j < m.columns(); j++)
        fmt::print("[{:^3}]", caption[j]);

    fmt::println("");

    for (std::size_t i = 0; i < m.rows(); i++) {
        fmt::print("[{:^3}]", caption[i]);

        for (std::size_t j = 0; j < m.columns(); j++) {
            if (i == j) {


                if (m.test(i, j)) {
                    fmt::print("[!!!]");
                    success = false;
                } else {
                    fmt::print("[---]");
                }
            } else if (m.test(i, j))
                fmt::print("[{:^3}]", '+');
            else
                fmt::print("[   ]");
        }

        fmt::println("");
    }

    return success;
}

inline std::string random_text ()
{
    lorem::lorem_ipsum ipsum;
    ipsum.set_paragraph_count(1);
    ipsum.set_sentence_count(10);
    ipsum.set_word_count(20);

    auto para = ipsum();
    std::string text;
    char const * delim = "" ;

    for (auto const & sentence: para[0]) {
        text += delim + sentence;
        delim = "\n";
    }

    return text;
}

} // namespace tools
