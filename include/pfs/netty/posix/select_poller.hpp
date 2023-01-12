////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/error.hpp"
#include <limits>
#include <sys/select.h>

namespace netty {
namespace posix {

class select_poller
{
public:
    using native_socket_type = int;

    // Highest-numbered file descriptor in any of the three sets.
    native_socket_type max_fd {-1};
    native_socket_type min_fd {(std::numeric_limits<native_socket_type>::max)()};

    fd_set readfds;
    fd_set writefds;

public:
    select_poller ();
    ~select_poller ();

    void add (native_socket_type sock, error * perr = nullptr);
    void remove (native_socket_type sock, error * perr = nullptr);
    bool empty () const noexcept;
    int poll (fd_set * rfds, fd_set * wfds, std::chrono::milliseconds millis, error * perr = nullptr);
};

}} // namespace netty::posix
