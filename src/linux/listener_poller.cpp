////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../listener_poller.hpp"

#if NETTY__EPOLL_ENABLED
#   include "pfs/netty/linux/epoll_poller.hpp"
#endif

#include <sys/socket.h>

namespace netty {

#if NETTY__EPOLL_ENABLED

template <>
listener_poller<linux_os::epoll_poller>::listener_poller (std::shared_ptr<linux_os::epoll_poller> ptr)
    : _rep(ptr == nullptr
        ? std::make_shared<linux_os::epoll_poller>(EPOLLERR | EPOLLIN | EPOLLRDNORM | EPOLLRDBAND)
        : std::move(ptr))
{
    init();
}

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
                          make_error_code(pfs::errc::system_error)
                        , tr::f_("get socket option failure: {}, listener socket removed: {}"
                            , pfs::system_error_text(), ev.data.fd)
                    });
                } else {
                    on_failure(ev.data.fd, error {
                          errc::socket_error
                        , tr::f_("accept socket error: {}, listener socket removed: {}"
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

#endif // NETTY__EPOLL_ENABLED

#if NETTY__EPOLL_ENABLED
template class listener_poller<linux_os::epoll_poller>;
#endif // NETTY__EPOLL_ENABLED

} // namespace netty
