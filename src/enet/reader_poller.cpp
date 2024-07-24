////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../reader_poller.hpp"
#include <pfs/assert.hpp>
#include <pfs/log.hpp>
#include <enet/enet.h>

#if NETTY__ENET_ENABLED
#   include "pfs/netty/enet/enet_poller.hpp"
#endif

namespace netty {

#if NETTY__ENET_ENABLED

template <>
reader_poller<enet::enet_poller>::reader_poller (std::shared_ptr<enet::enet_poller> ptr)
    : _rep(std::move(ptr))
{
    PFS__TERMINATE(_rep != nullptr, "ENet reader poller backend is null");
    init();
}

template <>
int reader_poller<enet::enet_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep->poll(millis, perr);

    if (n < 0)
        return n;

    n = 0;

    while (_rep->has_more_events()) {
        auto const & event = _rep->get_event();
        auto ev = reinterpret_cast<ENetEvent const *>(event.ev);

        if (ev->type == ENET_EVENT_TYPE_RECEIVE) {
            PFS__TERMINATE(ev->peer->data != nullptr, "ENet peer data is null");

            // printf ("A packet of length %u containing %s was received from %s on channel %u.\n",
            //      event.packet -> dataLength,
            //      event.packet -> data,
            //      event.peer -> data,
            //      event.channelID);

            auto * inpb_ptr = reinterpret_cast<enet::enet_socket::input_buffer_type *>(ev->peer->data);
            auto offset = inpb_ptr->size();
            inpb_ptr->resize(offset + ev->packet->dataLength);
            std::memcpy(inpb_ptr->data() + offset, ev->packet->data, ev->packet->dataLength);

            ready_read(event.sock);
            enet_packet_destroy(ev->packet);
            _rep->pop_event();
            n++;
        } else if (ev->type == ENET_EVENT_TYPE_DISCONNECT) {
            // Reset the peer's client information
            ev->peer->data = nullptr;

            disconnected(event.sock);
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
template class reader_poller<enet::enet_poller>;
#endif

} // namespace netty
