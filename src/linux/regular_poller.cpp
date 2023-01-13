////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../regular_poller.hpp"

#if NETTY__EPOLL_ENABLED
#   include "pfs/netty/linux/epoll_poller.hpp"
#endif

#include <sys/socket.h>

namespace netty {

#if NETTY__EPOLL_ENABLED

template <>
int regular_poller<linux::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(millis, perr);

    if (n < 0)
        return n;

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            auto revents = _rep.events[i].events;
            auto fd = _rep.events[i].data.fd;

            // There is data to read - can accept
            // Identical to `posix::poll_poller`
            if (revents & (EPOLLIN | EPOLLRDNORM | EPOLLRDBAND)) {
                bool disconnect = false;
                char buf[1];
                auto n = ::recv(fd, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);

                if (n > 0) {
                    if (ready_read)
                        ready_read(fd);
                } else if ( n == 0) {
                    disconnect = true;
                } else {
                    on_error(fd, tr::f_("read socket failure: {}"
                        , pfs::system_error_text(errno)));
                    disconnect = true;
                }

                if (disconnect) {
                    remove(fd);

                    if (disconnected)
                        disconnected(fd);
                }
            }

            // Writing is now possible, though a write larger than the available space
            // in a socket or pipe will still block (unless O_NONBLOCK is set).
            // Identical to `posix::poll_poller`
            if (revents & (EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND)) {
                if (can_write)
                    can_write(fd);
            }
        }
    }

    return n;
}

#endif // NETTY__EPOLL_ENABLED

#if NETTY__EPOLL_ENABLED
template class regular_poller<linux::epoll_poller>;
#endif

} // namespace netty
