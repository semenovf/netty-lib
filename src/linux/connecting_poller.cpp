////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../connecting_poller.hpp"

#if NETTY__EPOLL_ENABLED
#   include "pfs/netty/linux/epoll_poller.hpp"
#endif

#include <sys/socket.h>

namespace netty {

#if NETTY__EPOLL_ENABLED

template <>
int connecting_poller<linux_os::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(millis, perr);

    if (n < 0)
        return n;

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            auto revents = _rep.events[i].events;
            auto fd = _rep.events[i].data.fd;

            // 1. Occured while tcp socket attempts to connect to non-existance server socket (connection_refused)
            // 2. ... ?
            // Identical to `poll_poller`
            if (revents & EPOLLERR) {
                int error_val = 0;
                socklen_t len = sizeof(error_val);
                auto rc = getsockopt(fd, SOL_SOCKET, SO_ERROR, & error_val, & len);

                if (rc != 0) {
                    remove(fd);
                    on_error(fd, tr::f_("get socket option failure: {}, socket removed"
                        , pfs::system_error_text()));
                } else {
                    switch (error_val) {
                        case 0: // No error
                            break;

                        case ECONNREFUSED:
                            remove(fd);

                            if (connection_refused)
                                connection_refused(fd);

                            break;

                        default:
                            remove(fd);
                            on_error(fd, tr::_("unhandled error value returned by `getsockopt`"));
                            break;
                    }
                }

                continue;
            }

            // Hang up (output only).
            //
            // Contexts:
            // a. Attempt to connect to defunct server address/port
            // b. ...
            //
            // Identical to `poll_poller`
            if (revents & (EPOLLHUP | EPOLLRDHUP)) {
                remove(fd);

                if (connection_refused)
                    connection_refused(fd);

                continue;
            }

            // Writing is now possible, though a write larger than the available space
            // in a socket or pipe will still block (unless O_NONBLOCK is set).
            // Identical to `poll_poller`
            if (revents & (EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND)) {
                remove(fd);

                if (connected)
                    connected(fd);
            }
        }
    }

    return n;
}

#endif // #NETTY__EPOLL_ENABLED

#if NETTY__EPOLL_ENABLED
template class connecting_poller<linux_os::epoll_poller>;
#endif // NETTY__EPOLL_ENABLED

} // namespace netty
