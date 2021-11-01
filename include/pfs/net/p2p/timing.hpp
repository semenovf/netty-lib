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
#include <chrono>

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

}}} // namespace pfs::net::p2p


