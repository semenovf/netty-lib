////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.15 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../writer_poller.hpp"
#include <pfs/assert.hpp>
#include <pfs/log.hpp>
#include <enet/enet.h>

#if NETTY__ENET_ENABLED
#   include "pfs/netty/enet/enet_poller.hpp"
#endif

namespace netty {

#if NETTY__ENET_ENABLED

template <>
writer_poller<enet::enet_poller>::writer_poller (std::shared_ptr<enet::enet_poller> ptr)
    : _rep(std::move(ptr))
{
    PFS__TERMINATE(_rep != nullptr, "ENet writer poller backend is null");
    init();
}

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

#endif // NETTY__ENET_ENABLED

#if NETTY__ENET_ENABLED
template class writer_poller<enet::enet_poller>;
#endif // NETTY__ENET_ENABLED

} // namespace netty

