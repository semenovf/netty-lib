////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../listener_poller.hpp"
#include <pfs/log.hpp>
#include <enet/enet.h>

#if NETTY__ENET_ENABLED
#   include "pfs/netty/enet/enet_poller.hpp"
#endif

namespace netty {

#if NETTY__ENET_ENABLED

template <>
listener_poller<enet::enet_poller>::listener_poller (specialized)
{}

template <>
int listener_poller<enet::enet_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(millis, perr);

    if (n < 0)
        return n;

    if (n > 0) {
        while (_rep.has_more_events()) {
            auto const & event = _rep.get_event();
            auto ev = reinterpret_cast<ENetEvent const *>(event.ev);

            switch (ev->type) {
                case ENET_EVENT_TYPE_CONNECT:
                    LOG_TRACE_2("Accepted from: {}:{}", ev->peer->address.host, ev->peer->address.port);
                    accept(event.sock);
                    break;

                // FIXME
                case ENET_EVENT_TYPE_DISCONNECT:
                    LOG_TRACE_2("FIXME: listener_poller: ENET_EVENT_TYPE_DISCONNECT: {}:{}"
                        , ev->peer->address.host, ev->peer->address.port);

                    // Reset the peer's client information
                    ev->peer->data = nullptr;
                    break;

                // Ignore event in connecting poller
                case ENET_EVENT_TYPE_RECEIVE:
                    LOG_TRACE_2("FIXME: listener_poller: ENET_EVENT_TYPE_RECEIVE");
                    enet_packet_destroy(ev->packet);
                    break;

                case ENET_EVENT_TYPE_NONE:
                default:
                    break;
            }

            _rep.pop_event();
        }
    }

    return n;
}

#endif // NETTY__ENET_ENABLED

#if NETTY__ENET_ENABLED
template class listener_poller<enet::enet_poller>;
#endif

} // namespace netty

