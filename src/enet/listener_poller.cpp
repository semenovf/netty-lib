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
listener_poller<enet::enet_poller>::listener_poller (std::shared_ptr<enet::enet_poller> ptr)
    : _rep(std::move(ptr))
{
    PFS__TERMINATE(_rep != nullptr, "ENet listener poller backend is null");
    init();
}

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

            LOG_TRACE_2("Accepted from: {}", to_string(saddr));
            accept(reinterpret_cast<native_listener_type>(ev->peer)); // <= ENetPeer *
            _rep->pop_event();
            n++;
        } else {
            break;
        }
    }

    return n;
}

#endif // NETTY__ENET_ENABLED

#if NETTY__ENET_ENABLED
template class listener_poller<enet::enet_poller>;
#endif

} // namespace netty
