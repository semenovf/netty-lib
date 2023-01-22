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
int listener_poller<linux::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(millis, perr);

    if (n < 0)
        return n;

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            auto revents = _rep.events[i].events;
            auto fd = _rep.events[i].data.fd;

            if (revents & EPOLLERR) {
                int error_val = 0;
                socklen_t len = sizeof(error_val);
                auto rc = getsockopt(fd, SOL_SOCKET, SO_ERROR, & error_val, & len);

                remove(fd);

                if (rc != 0) {
                    on_error(fd, tr::f_("get socket option failure: {}, listener socket removed: {}"
                        , pfs::system_error_text(), fd));
                } else {
                    on_error(fd, tr::f_("accept socket error: {}, listener socket removed: {}"
                        , pfs::system_error_text(error_val), fd));
                }

                continue;
            }

            // There is data to read - can accept
            // Identical to `poll_poller`
            if (revents & (EPOLLIN | EPOLLRDNORM | EPOLLRDBAND)) {
                if (accept)
                    accept(fd);
            }
        }
    }

    return n;
}

#endif // NETTY__EPOLL_ENABLED

#if NETTY__EPOLL_ENABLED
template class listener_poller<linux::epoll_poller>;
#endif // NETTY__EPOLL_ENABLED

} // namespace netty
