////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../error.hpp"
#include "../chrono.hpp"
#include <map>
#include <set>

namespace netty {
namespace udt {

class epoll_poller
{
public:
    using socket_id = int;
    using listener_id = socket_id;

public:
    int eid {-1}; // -1 is same as CUDT::ERROR
    int counter {0}; // number of sockets polled currently
    std::set<socket_id> readfds;
    std::set<socket_id> writefds;
    bool observe_read {false};
    bool observe_write {false};

    // Need to control expiration timeout for connecting sockets.
    std::map<socket_id, clock_type::time_point> _connecting_sockets;

public:
    epoll_poller (bool observe_read, bool observe_write);
    ~epoll_poller ();

    void add_socket (socket_id sock, error * perr = nullptr);
    void add_listener (listener_id sock, error * perr = nullptr);
    void wait_for_write (socket_id sock, error * perr = nullptr);
    void remove_socket (socket_id sock, error * perr = nullptr);
    void remove_listener (listener_id sock, error * perr = nullptr);
    bool empty () const noexcept;
    int poll (int eid, std::set<socket_id> * readfds
        , std::set<socket_id> * writefds
        , std::chrono::milliseconds millis, error * perr = nullptr);
};

}} // namespace netty::udt
