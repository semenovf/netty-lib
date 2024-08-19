////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../connecting_poller.hpp"
#include "netty/socket4_addr.hpp"
#include <pfs/assert.hpp>
#include <pfs/endian.hpp>
#include <pfs/log.hpp>
#include <enet/enet.h>

#if NETTY__ENET_ENABLED
#   include "pfs/netty/enet/enet_poller.hpp"
#endif

namespace netty {

#if NETTY__ENET_ENABLED

template <>
connecting_poller<enet::enet_poller>::connecting_poller (std::shared_ptr<enet::enet_poller> ptr)
    : _rep(std::move(ptr))
{
    PFS__TERMINATE(_rep != nullptr, "ENet connecting poller backend is null");
    init();
}

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

            LOG_TRACE_1("Connected to: {}", to_string(saddr));

            connected(event.sock);
            _rep->pop_event();
            n++;
        } else if (ev->type == ENET_EVENT_TYPE_DISCONNECT) {
            socket4_addr saddr {
                  inet4_addr {pfs::to_native_order(ev->peer->address.host)}
                , ev->peer->address.port
            };

            LOG_TRACE_1("Disconnected from: {}", to_string(saddr));

            // Reset the peer's client information
            ev->peer->data = nullptr;
            connection_refused(event.sock, false);
            _rep->pop_event();
            n++;
        } else if (ev->type == ENET_EVENT_TYPE_RECEIVE) {
            LOG_TRACE_1("FIXME: connecting_poller: ENET_EVENT_TYPE_RECEIVE: unexpected event");
            break;
        } else {
            break;
        }
    }

    return n;
}

#endif // NETTY__ENET_ENABLED

#if NETTY__ENET_ENABLED
template class connecting_poller<enet::enet_poller>;
#endif

} // namespace netty
