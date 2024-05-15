////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/error.hpp"

namespace netty {
namespace enet {

class enet_poller
{
public:
    using native_socket_type = int;

public:
    enet_poller (/*bool observe_read, bool observe_write*/);
    ~enet_poller ();

//     void add (native_socket_type sock, error * perr = nullptr);
//     void remove (native_socket_type sock, error * perr = nullptr);
//     bool empty () const noexcept;
//     int poll (int eid, std::set<native_socket_type> * readfds
//         , std::set<native_socket_type> * writefds
//         , std::chrono::milliseconds millis, error * perr = nullptr);
};

}} // namespace netty::enet

