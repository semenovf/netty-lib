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
int listener_poller<udt::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(_rep.eid, & _rep.readfds, nullptr, millis, perr);

    if (n < 0)
        return n;

    if (n > 0) {
        for (UDTSOCKET u: _rep.readfds) {
            auto status = UDT::getsockstate(u);
            LOGD(TAG, "UDT server socket state: {}", status);

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
