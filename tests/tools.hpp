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
#include <array>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <thread>
#include <signal.h>

namespace tools {

#ifdef DOCTEST_VERSION
// See https://github.com/doctest/doctest/issues/345
inline char const * current_doctest_name ()
{
    return doctest::detail::g_cs->currentTest->m_name;
}
#endif

inline void sleep (int timeout, std::string const & description = std::string{})
{
    if (description.empty()) {
        LOGD("", "Waiting for {} seconds", timeout);
    } else {
        LOGD("", "{}: waiting for {} seconds", description, timeout);
    }

    std::this_thread::sleep_for(std::chrono::seconds{timeout});
}

inline void sleep_ms (int timeout)
{
    std::this_thread::sleep_for(std::chrono::milliseconds{timeout});
}

bool wait_atomic_bool (std::atomic_bool & flag
    , std::chrono::milliseconds timelimit = std::chrono::milliseconds{5000})
{
    pfs::countdown_timer<std::milli> timer {timelimit};

    while (!flag.load() && timer.remain_count() > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

    return flag.load();
}

// DEPRECATED use lorem::wait_atomic_counter
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

template <typename AtomicCounter, std::size_t N>
bool wait_atomic_counters (std::array<AtomicCounter, N> & counters
    , typename AtomicCounter::value_type limit
    , std::chrono::milliseconds timelimit = std::chrono::milliseconds{5000})
{
    pfs::countdown_timer<std::milli> timer {timelimit};

    bool success = false;

    while (!success && timer.remain_count() > 0) {
        success = true;

        for (std::size_t i = 0; success && i < N; i++)
            success = (counters[i].load() >= limit);

        if (!success)
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    return success;
}

template <typename SafeMatrix>
bool wait_matrix_count (SafeMatrix & safe_matrix, std::size_t limit
    , std::chrono::milliseconds timelimit = std::chrono::milliseconds{5000})
{
    pfs::countdown_timer<std::milli> timer {timelimit};

    while (safe_matrix.rlock()->count() < limit && timer.remain_count() > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

    return !(safe_matrix.rlock()->count() < limit);
}

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
    ipsum.set_word_count(10);

    auto para = ipsum();
    std::string text;
    char const * delim = "" ;

    for (auto const & sentence: para[0]) {
        text += delim + sentence;
        delim = "\n";
    }

    return text;
}

inline std::string random_small_text ()
{
    lorem::lorem_ipsum ipsum;
    ipsum.set_paragraph_count(1);
    ipsum.set_sentence_count(1);
    ipsum.set_word_count(1);

    auto para = ipsum();
    return para[0][0];
}

inline std::string random_large_text ()
{
    lorem::lorem_ipsum ipsum;
    ipsum.set_paragraph_count(1);
    ipsum.set_sentence_count(900);
    ipsum.set_word_count(100);

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
