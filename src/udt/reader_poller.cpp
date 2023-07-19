////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../reader_poller.hpp"

#if NETTY__UDT_ENABLED
#   include "newlib/udt.hpp"
#   include "pfs/netty/udt/epoll_poller.hpp"
#endif

namespace netty {

#if NETTY__UDT_ENABLED

template <>
reader_poller<udt::epoll_poller>::reader_poller (specialized)
    : _rep(true, false)
{}

template <>
int reader_poller<udt::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(_rep.eid, & _rep.readfds, nullptr, millis, perr);

    if (n < 0)
        return n;

    auto res = 0;

    if (n > 0) {
        if (!_rep.readfds.empty()) {
            for (UDTSOCKET u: _rep.readfds) {
                auto state = UDT::getsockstate(u);

                if (state == CONNECTED || state == OPENED) {
                    res++;
                    ready_read(u);
                } else {
                    if (state == CLOSED) {
                        disconnected(u)
                    } else {
                        on_failure(u, tr::f_("read socket failure: state={}"
                            " (TODO: handle properly)", state));
                    }
                }
            }
        }
    }

    return res;
}

#endif // NETTY__UDT_ENABLED

#if NETTY__UDT_ENABLED
template class reader_poller<udt::epoll_poller>;
#endif

} // namespace netty

