////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../listener_poller.hpp"

#if NETTY__UDT_ENABLED
#   include "newlib/udt.hpp"
#   include "pfs/netty/udt/epoll_poller.hpp"
#endif

#include "pfs/log.hpp"

static char const * TAG = "UDT";

namespace netty {

#if NETTY__UDT_ENABLED

template <>
listener_poller<udt::epoll_poller>::listener_poller (std::shared_ptr<udt::epoll_poller> ptr)
    : _rep(ptr == nullptr
        ? std::make_shared<udt::epoll_poller>(true, false)
        : std::move(ptr))
{
    init();
}

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
            LOGD(TAG, "Socket ACCEPTED: listener sock={}; state={}", u, static_cast<int>(status));

            res++;

            if (accept)
                accept(u);
        }
    }

    return n;
}

#endif // NETTY__UDT_ENABLED

#if NETTY__UDT_ENABLED
template class listener_poller<udt::epoll_poller>;
#endif

} // namespace netty
