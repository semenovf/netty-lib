////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../writer_poller_impl.hpp"
#include "newlib/udt.hpp"
#include "netty/udt/epoll_poller.hpp"

NETTY__NAMESPACE_BEGIN

template <>
writer_poller<udt::epoll_poller>::writer_poller ()
    : _rep(new udt::epoll_poller(false, true))
{}

template <>
int writer_poller<udt::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep->poll(_rep->eid, nullptr, & _rep->writefds, millis, perr);

    if (n < 0)
        return n;

    auto rc = 0;

    if (n > 0) {
        if (!_rep->writefds.empty()) {
            for (UDTSOCKET u: _rep->writefds) {
                rc++;

                //auto state = UDT::getsockstate(u);
                //LOGD(TAG, "UDT write socket state: {}", state);

                can_write(u);
            }
        }
    }

    return rc;
}

template class writer_poller<udt::epoll_poller>;

NETTY__NAMESPACE_END
