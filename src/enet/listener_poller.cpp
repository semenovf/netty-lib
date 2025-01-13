////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../listener_poller.hpp"
#include "netty/socket4_addr.hpp"
#include "netty/enet/enet_poller.hpp"
#include <pfs/endian.hpp>
#include <enet/enet.h>

#include <pfs/log.hpp>
#include "netty/trace.hpp"

NETTY__NAMESPACE_BEGIN

template <>
listener_poller<enet::enet_poller>::listener_poller ()
    : _rep(new enet::enet_poller)
{}

template <>
int listener_poller<enet::enet_poller>::poll (std::chrono::milliseconds millis, error * perr)
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

            NETTY__TRACE(LOGD("ENet", "Accepted from: {}", to_string(saddr)));

            accept(reinterpret_cast<socket_id>(ev->peer)); // <= ENetPeer *
            _rep->pop_event();
            n++;
        } else {
            break;
        }
    }

    return n;
}

template class listener_poller<enet::enet_poller>;

NETTY__NAMESPACE_END
