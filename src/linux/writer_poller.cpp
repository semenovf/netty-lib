////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../writer_poller.hpp"

#if NETTY__EPOLL_ENABLED
#   include "pfs/netty/linux/epoll_poller.hpp"
#endif

#include <sys/socket.h>

namespace netty {

#if NETTY__EPOLL_ENABLED

template <>
writer_poller<linux_os::epoll_poller>::writer_poller (specialized)
    : _rep(EPOLLERR | EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND)
{}

template <>
int writer_poller<linux_os::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(millis, perr);

    if (n < 0)
        return n;

    int res = 0;

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            auto revents = _rep.events[i].events;
            auto fd = _rep.events[i].data.fd;

            // This event is also reported for the write end of a pipe when
            // the read end has been closed.
            // TODO Recognize disconnection (EPIPE ?)
            if (revents & EPOLLERR) {
                int error_val = 0;
                socklen_t len = sizeof(error_val);
                auto rc = getsockopt(fd, SOL_SOCKET, SO_ERROR, & error_val, & len);

                remove(fd);

                if (rc != 0) {
                    on_error(fd, tr::f_("get socket option failure: {}, socket removed: {}"
                        , pfs::system_error_text(), fd));
                } else {
                    on_error(fd, tr::f_("write socket failure: {}, socket removed: {}"
                        , pfs::system_error_text(error_val), fd));
                }

                continue;
            }

            // Writing is now possible, though a write larger than the available space
            // in a socket or pipe will still block (unless O_NONBLOCK is set).
            // Identical to `posix::poll_poller`
            if (revents & (EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND)) {
                res++;

                remove(fd);

                if (can_write)
                    can_write(fd);
            }
        }
    }

    return res;
}

#endif // #NETTY__EPOLL_ENABLED

#if NETTY__EPOLL_ENABLED
template class writer_poller<linux_os::epoll_poller>;
#endif // NETTY__EPOLL_ENABLED

} // namespace netty


