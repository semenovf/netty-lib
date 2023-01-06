////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <vector>
#include <poll.h>

namespace netty {
namespace posix {

struct poll_poller
{
    using native_socket_type = int;
    std::vector<pollfd> events;
};

}} // namespace netty::posix

