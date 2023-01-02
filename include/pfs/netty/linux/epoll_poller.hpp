////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/error.hpp"
#include <chrono>
#include <vector>
#include <sys/epoll.h>

namespace netty {
namespace linux_ns {

struct epoll_poller
{
    using native_type = int;
    int eid {-1};
    std::vector<epoll_event> events;
};

}} // namespace netty::linux_ns
