////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#if NETTY__EPOLL_ENABLED
#include "../listener_poller_impl.hpp"
#include "netty/namespace.hpp"
#include "netty/linux/epoll_poller.hpp"
#include <pfs/i18n.hpp>
#include <sys/socket.h>

NETTY__NAMESPACE_BEGIN

template <>
listener_poller<linux_os::epoll_poller>::listener_poller ()
    : _rep(new linux_os::epoll_poller(EPOLLERR | EPOLLIN | EPOLLRDNORM | EPOLLRDBAND))
{}

template <>
int listener_poller<linux_os::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep->poll(millis, perr);

    if (n < 0)
        return n;

    int res = 0;

    if (n > 0) {
        for (auto const & ev: _rep->events) {
            if (n == 0)
                break;

            if (ev.events == 0)
                continue;

            n--;

            if (ev.events & EPOLLERR) {
                int error_val = 0;
                socklen_t len = sizeof(error_val);
                auto rc = getsockopt(ev.data.fd, SOL_SOCKET, SO_ERROR, & error_val, & len);

                if (rc != 0) {
                    on_failure(ev.data.fd, error {
                        tr::f_("get socket option failure: {}, listener socket removed: {}"
                            , pfs::system_error_text(), ev.data.fd)
                    });
                } else {
                    on_failure(ev.data.fd, error {
                        tr::f_("accept socket error: {}, listener socket removed: {}"
                            , pfs::system_error_text(error_val), ev.data.fd)
                    });
                }

                continue;
            }

            // There is data to read - can accept
            // Identical to `poll_poller`
            if (ev.events & (EPOLLIN | EPOLLRDNORM | EPOLLRDBAND)) {
                res++;
                accept(ev.data.fd);
            }
        }
    }

    return res;
}

template class listener_poller<linux_os::epoll_poller>;

NETTY__NAMESPACE_END

#endif // NETTY__EPOLL_ENABLED
