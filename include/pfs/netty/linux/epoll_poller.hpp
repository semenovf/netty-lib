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
#include <vector>
#include <sys/epoll.h>

namespace netty {
namespace linux_os {

class epoll_poller
{
public:
    using native_socket_type = int;
    using native_listener_type = native_socket_type;

public:
    int eid {-1};
    std::vector<epoll_event> events;
    std::uint32_t oevents; // Observable events

public:
    epoll_poller (std::uint32_t observable_events);
    ~epoll_poller ();

    void add_socket (native_socket_type sock, error * perr = nullptr);
    void add_listener (native_listener_type sock, error * perr = nullptr);
    void wait_for_write (native_socket_type sock, error * perr = nullptr);
    void remove_socket (native_socket_type sock, error * perr = nullptr);
    void remove_listener (native_listener_type sock, error * perr = nullptr);
    bool empty () const noexcept;
    int poll (std::chrono::milliseconds millis, error * perr = nullptr);
};

}} // namespace netty::linux_os
