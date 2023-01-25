////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../listener_poller.hpp"

#if NETTY__SELECT_ENABLED
#   include "pfs/netty/posix/select_poller.hpp"
#endif

#if NETTY__POLL_ENABLED
#   include "pfs/netty/posix/poll_poller.hpp"
#endif

#if _MSC_VER
#   include <winsock2.h>
#else
#   include <sys/types.h>
#   include <sys/select.h>
#   include <sys/socket.h>
#endif

namespace netty {

#if NETTY__SELECT_ENABLED

template <>
listener_poller<posix::select_poller>::listener_poller (specialized)
    : _rep(true, false)
{}

template <>
int listener_poller<posix::select_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    fd_set rfds;

    auto n = _rep.poll(& rfds, nullptr, millis, perr);

    if (n < 0)
        return n;

    int res = 0;

    if (n > 0) {
        int rcounter = n;

        for (int fd = _rep.min_fd; fd <= _rep.max_fd && rcounter > 0; fd++) {
            if (FD_ISSET(fd, & rfds)) {
                res++;

                if (accept)
                    accept(fd);

                --rcounter;
            }
        }
    }

    return res;
}

#endif // NETTY__SELECT_ENABLED

#if NETTY__POLL_ENABLED

template <>
listener_poller<posix::poll_poller>::listener_poller (specialized)
    : _rep(POLLERR | POLLIN

#ifdef POLLRDNORM
        | POLLRDNORM
#endif

#ifdef POLLRDBAND
        | POLLRDBAND
#endif
    )
{}

template <>
int listener_poller<posix::poll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(millis, perr);

    if (n < 0)
        return n;

    int res = 0;

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            auto revents = _rep.events[i].revents;
            auto fd = _rep.events[i].fd;

            // 1. Occured while tcp socket attempts to connect to non-existance server socket (connection_refused)
            // 2. ... ?
            // Identical to `epoll_poller`
            if (revents & POLLERR) {
                int error_val = 0;
                socklen_t len = sizeof(error_val);
                auto rc = getsockopt(fd, SOL_SOCKET, SO_ERROR, & error_val, & len);

                remove(fd);

                if (rc != 0) {
                    on_error(fd, tr::f_("get socket option failure: {}, socket removed: {}"
                        , pfs::system_error_text(), fd));
                } else {
                    on_error(fd, tr::f_("accept socket error: {}, socket removed: {}"
                        , pfs::system_error_text(error_val), fd));
                }

                continue;
            }

            // There is data to read - can accept
            // Identical to `epoll_poller`
            if (revents & (POLLIN
#ifdef POLLRDNORM
                | POLLRDNORM
#endif
#ifdef POLLRDBAND
                | POLLRDBAND
#endif
            )) {
                res++;

                if (accept)
                    accept(fd);
            }
        }
    }

    return res;
}

#endif // NETTY__POLL_ENABLED

#if NETTY__SELECT_ENABLED
template class listener_poller<posix::select_poller>;
#endif

#if NETTY__POLL_ENABLED
template class listener_poller<posix::poll_poller>;
#endif

} // namespace netty
