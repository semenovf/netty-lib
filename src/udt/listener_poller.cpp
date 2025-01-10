////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../listener_poller.hpp"
#include "newlib/udt.hpp"
#include "netty/udt/epoll_poller.hpp"
#include "netty/trace.hpp"
#include <pfs/log.hpp>

NETTY__NAMESPACE_BEGIN

template <>
listener_poller<udt::epoll_poller>::listener_poller ()
    : _rep(new udt::epoll_poller(true, false))
{}

template <>
int listener_poller<udt::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep->poll(_rep->eid, & _rep->readfds, nullptr, millis, perr);

    if (n < 0)
        return n;

    int res = 0;

    if (n > 0) {
        for (UDTSOCKET u: _rep->readfds) {
            auto status = UDT::getsockstate(u);

            NETTY__TRACE(LOGD("UDT", "Socket ACCEPTED: listener sock={}; state={}", u
                , static_cast<int>(status)));

            res++;

            if (accept)
                accept(u);
        }
    }

    return n;
}

template class listener_poller<udt::epoll_poller>;

NETTY__NAMESPACE_END
