////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.20 Initial version.
//      2021.11.01 Complete basic version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <chrono>

namespace netty {

using clock_type = std::chrono::steady_clock;

inline clock_type::time_point current_timepoint ()
{
    return clock_type::now();
}

inline clock_type::time_point future_timepoint (std::chrono::milliseconds increment)
{
    return current_timepoint() + increment;
}

inline bool timepoint_expired (clock_type::time_point sample)
{
    return current_timepoint() > sample;
}

inline std::chrono::milliseconds current_millis ()
{
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    return duration_cast<milliseconds>(clock_type::now().time_since_epoch());
}

inline std::chrono::milliseconds millis_since_epoch (clock_type::time_point const & tp)
{
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    return duration_cast<milliseconds>(tp.time_since_epoch());
}

inline std::chrono::seconds seconds_since_epoch (clock_type::time_point const & tp)
{
    using std::chrono::duration_cast;
    using std::chrono::seconds;

    return duration_cast<seconds>(tp.time_since_epoch());
}

} // namespace netty
