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
#include "enet_listener.hpp"
#include "pfs/netty/error.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <queue>
#include <set>
#include <vector>

struct _ENetHost;

namespace netty {
namespace enet {

class enet_poller
{
public:
    using native_socket_type   = enet_socket::native_type;
    using native_listener_type = enet_listener::native_type;

    struct event_item
    {
        native_socket_type sock;
        char ev[32]; // Must be enough to store ENetEvent;
    };

private:
    std::queue<event_item> _events;
    std::vector<native_socket_type> _sockets;
    std::vector<native_listener_type> _listeners;
    std::set<native_socket_type> _wait_for_write_sockets;

private:
    int poll_helper (_ENetHost * host, std::chrono::milliseconds millis, error * perr);

public:
    enet_poller ();
    ~enet_poller ();

    void add_socket (native_socket_type sock, error * perr = nullptr);
    void add_listener (native_listener_type sock, error * perr = nullptr);
    void wait_for_write (native_socket_type sock, error * perr = nullptr);
    void remove_socket (native_socket_type sock, error * perr = nullptr);
    void remove_listener (native_listener_type sock, error * perr = nullptr);
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

    std::size_t socket_count () const
    {
        return _sockets.size();
    }

    bool has_wait_for_write_sockets () const noexcept
    {
        return !_wait_for_write_sockets.empty();
    }

    /**
     * @return number of sockets can write.
     */
    int notify_can_write (std::function<void (native_socket_type)> && can_write);
};

}} // namespace netty::enet
