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
#include <atomic>
#include <cstdint>
#include <chrono>
#include <thread>
#include <signal.h>

namespace tools {

inline void sleep (int timeout, std::string const & description = std::string{})
{
    if (description.empty()) {
        LOGD(TAG, "Waiting for {} seconds", timeout);
    } else {
        LOGD(TAG, "{}: waiting for {} seconds", description, timeout);
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

} // namespace tools
