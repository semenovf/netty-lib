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

#if NETTY__POLL_ENABLED
#   include <poll.h>
#endif

namespace netty {
namespace posix {

#if NETTY__POLL_ENABLED

class poll_poller
{
public:
    using socket_id = int;
    using listener_id = socket_id;

public:
    std::vector<pollfd> events;
    short int oevents; // Observable events

public:
    poll_poller (short int observable_events);
    ~poll_poller ();

    void add_socket (socket_id sock, error * perr = nullptr);
    void add_listener (listener_id sock, error * perr = nullptr);
    void wait_for_write (socket_id sock, error * perr = nullptr);
    void remove_socket (socket_id sock, error * perr = nullptr);
    void remove_listener (listener_id sock, error * perr = nullptr);
    bool empty () const noexcept;
    int poll (std::chrono::milliseconds millis, error * perr = nullptr);
};
#endif

}} // namespace netty::posix

