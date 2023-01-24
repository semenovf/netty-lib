////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../writer_poller.hpp"

#if NETTY__SELECT_ENABLED
#   include "pfs/netty/posix/select_poller.hpp"
#endif

#if NETTY__POLL_ENABLED
#   include "pfs/netty/posix/poll_poller.hpp"
#endif

#include <sys/socket.h>

#include "pfs/log.hpp"

static char const * TAG = "POSIX";

namespace netty {

#if NETTY__SELECT_ENABLED

template <>
writer_poller<posix::select_poller>::writer_poller (specialized)
    : _rep(false, true)
{}

template <>
int writer_poller<posix::select_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    fd_set wfds;

    auto n = _rep.poll(nullptr, & wfds, millis, perr);

    if (n < 0)
        return n;

    auto res = 0;

    if (n > 0) {
        int rcounter = n;

        for (int fd = _rep.min_fd; fd <= _rep.max_fd && rcounter > 0; fd++) {
            if (FD_ISSET(fd, & wfds)) {
                res++;

                remove(fd);

                if (can_write)
                    can_write(fd);

                --rcounter;
            }
        }
    }

    return res;
}

#endif // NETTY__SELECT_ENABLED

#if NETTY__POLL_ENABLED

template <>
writer_poller<posix::poll_poller>::writer_poller (specialized)
    : _rep(POLLERR | POLLOUT

#ifdef POLLWRNORM
        | POLLWRNORM
#endif

#ifdef POLLWRBAND
        | POLLWRBAND
#endif
    )

{}

template <>
int writer_poller<posix::poll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(millis, perr);

    if (n < 0)
        return n;

    auto res = 0;

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            auto revents = _rep.events[i].revents;
            auto fd = _rep.events[i].fd;

            // This event is also reported for the write end of a pipe when
            // the read end has been closed.
            // TODO Recognize disconnection (EPIPE ?)
            if (revents & POLLERR) {
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
            // Identical to `epoll_poller`
            if (revents & (POLLOUT
#ifdef POLLWRNORM
                | POLLWRNORM
#endif
#ifdef POLLWRBAND
                | POLLWRBAND
#endif
            )) {
                res++;

                remove(fd);

                if (can_write)
                    can_write(fd);
            }
        }
    }

    return res;
}

#endif // NETTY__POLL_ENABLED

#if NETTY__SELECT_ENABLED
template class writer_poller<posix::select_poller>;
#endif

#if NETTY__POLL_ENABLED
template class writer_poller<posix::poll_poller>;
#endif

} // namespace netty
