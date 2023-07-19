////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../writer_poller.hpp"

#if NETTY__UDT_ENABLED
#   include "newlib/udt.hpp"
#   include "pfs/netty/udt/epoll_poller.hpp"
#endif

namespace netty {

#if NETTY__UDT_ENABLED

template <>
writer_poller<udt::epoll_poller>::writer_poller (specialized)
    : _rep(false, true)
{}

template <>
int writer_poller<udt::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(_rep.eid, nullptr, & _rep.writefds, millis, perr);

    if (n < 0)
        return n;

    auto res = 0;

    if (n > 0) {
        if (!_rep.writefds.empty()) {
            for (UDTSOCKET u: _rep.writefds) {
                res++;

                //auto state = UDT::getsockstate(u);
                //LOGD(TAG, "UDT write socket state: {}", state);

                can_write(u);
            }
        }
    }

    return res;
}

#endif // NETTY__UDT_ENABLED

#if NETTY__UDT_ENABLED
template class writer_poller<udt::epoll_poller>;
#endif

} // namespace netty
