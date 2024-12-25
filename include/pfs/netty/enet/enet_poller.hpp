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
    using socket_id   = enet_socket::native_type;
    using listener_id = enet_listener::native_type;

    struct event_item
    {
        socket_id sock;
        char ev[32]; // Must be enough to store ENetEvent;
    };

private:
    std::queue<event_item> _events;
    std::vector<socket_id> _sockets;
    std::vector<listener_id> _listeners;
    std::set<socket_id> _wait_for_write_sockets;

private:
    int poll_helper (_ENetHost * host, std::chrono::milliseconds millis, error * perr);

public:
    enet_poller ();
    ~enet_poller ();

    void add_socket (socket_id sock, error * perr = nullptr);
    void add_listener (listener_id sock, error * perr = nullptr);
    void wait_for_write (socket_id sock, error * perr = nullptr);
    void remove_socket (socket_id sock, error * perr = nullptr);
    void remove_listener (listener_id sock, error * perr = nullptr);
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
    int check_and_notify_can_write (std::function<void (socket_id)> && can_write);
};

}} // namespace netty::enet
