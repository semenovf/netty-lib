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
#include <set>

namespace netty {
namespace udt {

class epoll_poller
{
public:
    using native_socket_type = int;
    using native_listener_type = native_socket_type;

public:
    int eid {-1}; // -1 is same as CUDT::ERROR
    int counter {0}; // number of sockets polled currently
    std::set<native_socket_type> readfds;
    std::set<native_socket_type> writefds;
    bool observe_read {false};
    bool observe_write {false};

public:
    epoll_poller (bool observe_read, bool observe_write);
    ~epoll_poller ();

    void add (native_socket_type sock, error * perr = nullptr);
    void remove (native_socket_type sock, error * perr = nullptr);
    bool empty () const noexcept;
    int poll (int eid, std::set<native_socket_type> * readfds
        , std::set<native_socket_type> * writefds
        , std::chrono::milliseconds millis, error * perr = nullptr);
};

}} // namespace netty::udt
