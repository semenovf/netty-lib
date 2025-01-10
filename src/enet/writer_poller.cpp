////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../writer_poller.hpp"
#include "netty/enet/enet_poller.hpp"
#include <enet/enet.h>

#include "netty/trace.hpp"
#include <pfs/log.hpp>

NETTY__NAMESPACE_BEGIN

template <>
writer_poller<enet::enet_poller>::writer_poller ()
    : _rep(new enet::enet_poller)
{}

template <>
int writer_poller<enet::enet_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep->poll(millis, perr);

    if (n < 0)
        return n;

    n = 0;

    if (_rep->has_wait_for_write_sockets()) {
        n = _rep->check_and_notify_can_write([this] (socket_id sock) {
            can_write(sock);
        });
    }

    return n;
}

template class writer_poller<enet::enet_poller>;

NETTY__NAMESPACE_END
