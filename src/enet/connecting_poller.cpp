////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../connecting_poller.hpp"
#include "netty/socket4_addr.hpp"
#include "netty/enet/enet_poller.hpp"
#include <pfs/assert.hpp>
#include <pfs/endian.hpp>
#include <enet/enet.h>

#include <pfs/log.hpp>
#include "netty/trace.hpp"

NETTY__NAMESPACE_BEGIN

template <>
connecting_poller<enet::enet_poller>::connecting_poller ()
    : _rep(new enet::enet_poller)
{}

template <>
int connecting_poller<enet::enet_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep->poll(millis, perr);

    if (n < 0)
        return n;

    n = 0;

    while (_rep->has_more_events()) {
        auto const & event = _rep->get_event();
        auto ev = reinterpret_cast<ENetEvent const *>(event.ev);

        if (ev->type == ENET_EVENT_TYPE_CONNECT) {
            socket4_addr saddr {
                  inet4_addr {pfs::to_native_order(ev->peer->address.host)}
                , ev->peer->address.port
            };

            NETTY__TRACE(LOGD("ENet", "Connected to: {}", to_string(saddr)));

            connected(event.sock);
            _rep->pop_event();
            n++;
        } else if (ev->type == ENET_EVENT_TYPE_DISCONNECT) {
            socket4_addr saddr {
                  inet4_addr {pfs::to_native_order(ev->peer->address.host)}
                , ev->peer->address.port
            };

            NETTY__TRACE(LOGD("ENet", "Disconnected from: {}", to_string(saddr)));

            // Reset the peer's client information
            ev->peer->data = nullptr;
            connection_refused(event.sock, connection_refused_reason::other);
            _rep->pop_event();
            n++;
        } else if (ev->type == ENET_EVENT_TYPE_RECEIVE) {
            LOGE("ENet", "FIXME: connecting_poller: ENET_EVENT_TYPE_RECEIVE: unexpected event");
            break;
        } else {
            break;
        }
    }

    return n;
}

template class connecting_poller<enet::enet_poller>;

NETTY__NAMESPACE_END
