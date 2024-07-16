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

namespace netty {
namespace enet {

static_assert(sizeof(enet_poller::event_item::ev) >= sizeof(ENetEvent)
    , "increase size of `enet_poller::event_item::ev` to store `ENetEvent`");

enet_poller::enet_poller () = default;
enet_poller::~enet_poller () = default;

void enet_poller::add (native_socket_type sock, error * perr)
{
    _sockets.push_back(sock);
}

void enet_poller::remove (native_socket_type sock, error * /*perr*/)
{
    auto pos = std::find(_sockets.begin(), _sockets.end(), sock);
    _sockets.erase(pos);
}

bool enet_poller::empty () const noexcept
{
    return _sockets.empty();
}

int enet_poller::poll_helper (native_socket_type sock, std::chrono::milliseconds millis, error * perr)
{
    ENetEvent event;

    auto host = reinterpret_cast<ENetHost *>(sock);
    auto rc = enet_host_service(host, & event, millis.count());

    if (rc == 0)
        return 0;

    if (rc < 0) {
        pfs::throw_or(perr, error {
              errc::socket_error
            , tr::_("ENet poll failure")
        });

        return rc;
    }

    switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
        case ENET_EVENT_TYPE_RECEIVE:
        case ENET_EVENT_TYPE_DISCONNECT:
            LOG_TRACE_2("=== enet_poller::poll_helper() ===");
            _events.emplace();
            _events.back().sock = sock;
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

    pfs::stopwatch<std::milli, std::size_t> stopwatch;
    stopwatch.start();

    for (auto sock: _sockets) {
        auto n = poll_helper(sock, std::chrono::milliseconds{0}, perr);

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

    if (elapsed < millis) {
        auto remain_millis = millis - elapsed;

        auto n = poll_helper(_sockets.front(), remain_millis, perr);

        if (n < 0) {
            success = false;
        } else {
            total_events += n;
        }
    }

    return success ? total_events : -1;
}

}} // namespace netty::enet
