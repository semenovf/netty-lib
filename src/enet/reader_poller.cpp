////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../reader_poller_impl.hpp"
#include "netty/enet/enet_poller.hpp"
#include <pfs/assert.hpp>
#include <enet/enet.h>

#include "netty/trace.hpp"
#include <pfs/log.hpp>

NETTY__NAMESPACE_BEGIN

template <>
reader_poller<enet::enet_poller>::reader_poller ()
    : _rep(new enet::enet_poller)
{}

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

template class reader_poller<enet::enet_poller>;

NETTY__NAMESPACE_END
