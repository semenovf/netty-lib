////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../reader_poller_impl.hpp"
#include "newlib/udt.hpp"
#include "netty/udt/epoll_poller.hpp"

NETTY__NAMESPACE_BEGIN

template <>
reader_poller<udt::epoll_poller>::reader_poller ()
    : _rep(new udt::epoll_poller(true, false))
{}

template <>
int reader_poller<udt::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep->poll(_rep->eid, & _rep->readfds, nullptr, millis, perr);

    if (n < 0)
        return n;

    auto res = 0;

    if (n > 0) {
        if (!_rep->readfds.empty()) {
            for (UDTSOCKET u: _rep->readfds) {
                auto state = UDT::getsockstate(u);

                if (state == CONNECTED || state == OPENED) {
                    res++;
                    on_ready_read(u);
                } else {
                    // BROKEN - on connected socket when peer socket closed.
                    if (state == BROKEN || state == CLOSED) {
                        on_disconnected(u);
                    } else {
                        on_failure(u, error {
                            tr::f_("read socket failure: state={} (TODO: need to handle properly)"
                                , static_cast<int>(state))
                        });
                    }
                }
            }
        }
    }

    return res;
}

template class reader_poller<udt::epoll_poller>;

NETTY__NAMESPACE_END
