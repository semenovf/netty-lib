////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/error.hpp"
#include <vector>
#include <poll.h>

namespace netty {
namespace posix {

class poll_poller
{
public:
    using native_socket_type = int;
    using native_listener_type = native_socket_type;

public:
    std::vector<pollfd> events;
    short int oevents; // Observable events

public:
    poll_poller (short int observable_events);
    ~poll_poller ();

    void add_socket (native_socket_type sock, error * perr = nullptr);
    void add_listener (native_listener_type sock, error * perr = nullptr);
    void wait_for_write (native_socket_type sock, error * perr = nullptr);
    void remove_socket (native_socket_type sock, error * perr = nullptr);
    void remove_listener (native_listener_type sock, error * perr = nullptr);
    bool empty () const noexcept;
    int poll (std::chrono::milliseconds millis, error * perr = nullptr);
};

}} // namespace netty::posix

