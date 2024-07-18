////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "enet_socket.hpp"
#include "pfs/netty/error.hpp"
#include <chrono>
#include <memory>
#include <queue>
#include <vector>

namespace netty {
namespace enet {

class enet_poller
{
public:
    using native_socket_type = enet_socket::native_type;

    struct event_item
    {
        native_socket_type sock;
        char ev[32]; // Must be enough to store ENetEvent;
    };

private:
    std::queue<event_item> _events;
    std::vector<native_socket_type> _sockets;

private:
    int poll_helper (native_socket_type sock, std::chrono::milliseconds millis, error * perr);

public:
    enet_poller ();
    ~enet_poller ();

    void add (native_socket_type sock, error * perr = nullptr);
    void remove (native_socket_type sock, error * perr = nullptr);
    bool empty () const noexcept;
    int poll (std::chrono::milliseconds millis, error * perr = nullptr);

    bool has_more_events () const noexcept
    {
        return !_events.empty();
    }

    event_item const & get_event () const
    {
        return _events.front();
    }

    event_item next_event ()
    {
        event_item result = _events.front();
        _events.pop();
        return result;
    }

    void pop_event ()
    {
        _events.pop();
    }

    std::size_t event_count () const
    {
        return _events.size();
    }
};

}} // namespace netty::enet
