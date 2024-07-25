////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/enet/enet_poller.hpp"
#include <pfs/i18n.hpp>
#include <pfs/log.hpp>
#include <pfs/stopwatch.hpp>
#include <enet/enet.h>
#include <algorithm>
#include <thread>

namespace netty {
namespace enet {

static_assert(sizeof(enet_poller::event_item::ev) >= sizeof(ENetEvent)
    , "increase size of `enet_poller::event_item::ev` to store `ENetEvent`");

enet_poller::enet_poller () = default;
enet_poller::~enet_poller () = default;

void enet_poller::add_socket (native_socket_type sock, error * /*perr*/)
{
    auto pos = std::find(_sockets.begin(), _sockets.end(), sock);

    if (pos == _sockets.end())
        _sockets.push_back(sock);
}

void enet_poller::add_listener (native_listener_type sock, error * /*perr*/)
{
    auto pos = std::find(_listeners.begin(), _listeners.end(), sock);

    if (pos == _listeners.end())
        _listeners.push_back(sock);
}

void enet_poller::wait_for_write (native_socket_type sock, error * perr)
{
    this->add_socket(sock, perr);
    _wait_for_write_sockets.insert(sock);
}

void enet_poller::remove_socket (native_socket_type sock, error * /*perr*/)
{
    auto pos = std::find(_sockets.begin(), _sockets.end(), sock);

    if (pos != _sockets.end())
        _sockets.erase(pos);
}

void enet_poller::remove_listener (native_listener_type sock, error * perr)
{
    auto pos = std::find(_listeners.begin(), _listeners.end(), sock);

    if (pos != _listeners.end())
        _listeners.erase(pos);
}

bool enet_poller::empty () const noexcept
{
    return _sockets.empty() && _listeners.empty();
}

int enet_poller::poll_helper (ENetHost * host, std::chrono::milliseconds millis, error * perr)
{
    ENetEvent event;

    auto rc = enet_host_service(host, & event, millis.count());

    if (rc == 0)
        return 0;

    // For now believe that the error is not fatal
    if (rc < 0) {
        if (perr) {
            *perr = error {
                  errc::socket_error
                , tr::_("ENet poll (enet_host_service) error")
            };
        }

        return rc;
    }

    switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
        case ENET_EVENT_TYPE_RECEIVE:
        case ENET_EVENT_TYPE_DISCONNECT:
            _events.emplace();
            _events.back().sock = reinterpret_cast<native_socket_type>(event.peer);
            new (_events.back().ev) ENetEvent(std::move(event));
            break;

        case ENET_EVENT_TYPE_NONE:
        default:
            break;
    }

    return rc;
}

int enet_poller::poll (std::chrono::milliseconds millis, error * perr)
{
    bool success = true;
    int total_events = 0;

    if (empty())
        return 0;

    auto remain_millis = millis;

    do {
        pfs::stopwatch<std::milli, std::size_t> stopwatch;
        stopwatch.start();

        for (auto sock: _listeners) {
            auto host = reinterpret_cast<ENetHost *>(sock);

            auto n = poll_helper(host, std::chrono::milliseconds{0}, perr);

            if (n < 0) {
                success = false;
            } else {
                total_events += n;
            }
        }

        for (auto sock: _sockets) {
            auto peer = reinterpret_cast<ENetPeer *>(sock);
            auto host = peer->host;

            auto n = poll_helper(host, std::chrono::milliseconds{0}, perr);

            if (n < 0) {
                success = false;
            } else {
                total_events += n;
            }
        }

        stopwatch.stop();

        if (!success)
            return -1;

        if (total_events > 0)
            return total_events;

        std::chrono::milliseconds elapsed {stopwatch.count()};

        remain_millis -= elapsed;

        stopwatch.start();

        if (elapsed == std::chrono::milliseconds{0}) {
            std::chrono::milliseconds quant {10};

            if (remain_millis > quant) {
                std::this_thread::sleep_for(quant);
            } else {
                std::this_thread::sleep_for(remain_millis);
            }
        }

        stopwatch.stop();

        elapsed = std::chrono::milliseconds{stopwatch.count()};
        remain_millis -= elapsed;
    } while (remain_millis > std::chrono::milliseconds{0});

    return success ? total_events : -1;
}

int enet_poller::check_and_notify_can_write (std::function<void (native_socket_type)> && can_write)
{
    int n = 0;

    for (auto pos = _wait_for_write_sockets.begin(); pos != _wait_for_write_sockets.end();) {
        ENetPeer * peer = reinterpret_cast<ENetPeer *>(*pos);

        if (!enet_peer_has_outgoing_commands(peer)) {
            auto sock = *pos;
            pos = _wait_for_write_sockets.erase(pos);
            can_write(sock);
            n++;
        } else {
            ++pos;
        }
    }

    return n;
}

}} // namespace netty::enet
