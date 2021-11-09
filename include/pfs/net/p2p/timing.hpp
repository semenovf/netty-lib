////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.29 Initial version (utils.hpp).
//      2021.10.20 Initial version (timing.hpp).
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/fmt.hpp"
#include <chrono>
#include <string>

namespace pfs {
namespace net {
namespace p2p {

inline std::chrono::milliseconds current_timepoint ()
{
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::steady_clock;

    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch());
}

inline std::string to_string (std::chrono::milliseconds msecs)
{
    std::uint64_t count = msecs.count();
    int millis = count % 1000;
    std::uint64_t seconds = count / 1000;
    std::uint64_t hours   = seconds / 3600;
    seconds -= hours * 3600;
    std::uint64_t minutes = seconds / 60;
    seconds -= minutes * 60;

    //std::uint64_t sample = millis
    //    + (seconds + minutes * 60 + hours * 3600) * 1000;

    return fmt::format("{}:{:02}:{:02}.{:03}"
        , hours, minutes, seconds, millis);
}

}}} // namespace pfs::net::p2p


