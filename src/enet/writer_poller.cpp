////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../writer_poller.hpp"
#include <pfs/log.hpp>
#include <enet/enet.h>

#if NETTY__ENET_ENABLED
#   include "pfs/netty/enet/enet_poller.hpp"
#endif

namespace netty {

#if NETTY__EPOLL_ENABLED

template <>
writer_poller<enet::enet_poller>::writer_poller (specialized)
{}

template <>
int writer_poller<enet::enet_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(millis, perr);

    if (n < 0)
        return n;

    if (n > 0) {
        while (_rep.has_more_events()) {
            auto const & event = _rep.get_event();
            //auto ev = reinterpret_cast<ENetEvent const *>(event.ev);
            can_write(event.sock);

            // switch (ev->type) {
            //     // FIXME
            //     case ENET_EVENT_TYPE_CONNECT:
            //         LOG_TRACE_2("FIXME: writer_poller: ENET_EVENT_TYPE_CONNECT: {}:{}"
            //             , ev->peer->address.host, ev->peer->address.post);
            //         break;
            //
            //     // FIXME
            //     case ENET_EVENT_TYPE_DISCONNECT:
            //         LOG_TRACE_2("FIXME: writer_poller: ENET_EVENT_TYPE_DISCONNECT: {}:{}"
            //             , ev->peer->address.host, ev->peer->address.post);
            //
            //         // Reset the peer's client information
            //         ev->peer->data = nullptr;
            //         break;
            //
            //     case ENET_EVENT_TYPE_RECEIVE:
            //         LOG_TRACE_2("FIXME: writer_poller: ENET_EVENT_TYPE_RECEIVE");
            //         // Ignore event in connecting poller
            //         enet_packet_destroy(ev->packet);
            //         break;
            //
            //     case ENET_EVENT_TYPE_NONE:
            //     default:
            //         break;
            // }

            _rep.pop_event();
        }
    }

    return n;
}

#endif // NETTY__EPOLL_ENABLED

#if NETTY__EPOLL_ENABLED
template class writer_poller<enet::enet_poller>;
#endif // NETTY__EPOLL_ENABLED

} // namespace netty

