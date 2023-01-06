////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <limits>
#include <sys/select.h>

namespace netty {
namespace posix {

struct select_poller
{
    using native_socket_type = int;

    // Highest-numbered file descriptor in any of the three sets.
    native_socket_type max_fd {-1};
    native_socket_type min_fd {(std::numeric_limits<native_socket_type>::max)()};

    fd_set readfds;
    fd_set writefds;
};

}} // namespace netty::posix
