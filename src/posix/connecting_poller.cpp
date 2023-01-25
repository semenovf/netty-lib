////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../connecting_poller.hpp"

#if NETTY__SELECT_ENABLED
#   include "pfs/netty/posix/select_poller.hpp"
#endif

#if NETTY__POLL_ENABLED
#   include "pfs/netty/posix/poll_poller.hpp"
#endif

#if _MSC_VER
#   include <winsock2.h>
#else
#   include <sys/socket.h>
#endif

namespace netty {

#if NETTY__SELECT_ENABLED

template <>
connecting_poller<posix::select_poller>::connecting_poller (specialized)
    : _rep(true, true)
{}

template <>
int connecting_poller<posix::select_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    fd_set rfds;
    fd_set wfds;

    auto n = _rep.poll(& rfds, & wfds, millis, perr);

    if (n < 0)
        return n;

    int res = 0;

    if (n > 0) {
        int rcounter = n;

        for (int fd = _rep.min_fd; fd <= _rep.max_fd && rcounter > 0; fd++) {
            bool removed = false;

            if (FD_ISSET(fd, & rfds)) {
                int error_val = 0;
#if _MSC_VER
                int len = sizeof(error_val);
                auto rc = ::getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(& error_val), &len);

#else
                socklen_t len = sizeof(error_val);
                auto rc = ::getsockopt(fd, SOL_SOCKET, SO_ERROR, & error_val, & len);
#endif

                if (rc != 0) {
                    remove(fd);
                    removed = true;
                    on_error(fd, tr::f_("get socket option failure: {}, socket removed"
                        , pfs::system_error_text()));
                } else {
                    switch (error_val) {
                        case 0: // No error
                            break;

                        case ECONNREFUSED:
                            remove(fd);
                            removed = true;

                            if (connection_refused)
                                connection_refused(fd);

                            break;

                        default:
                            remove(fd);
                            removed = true;
                            on_error(fd, tr::_("unhandled error value returned by `getsockopt`"));
                            break;
                    }
                }

                --rcounter;
            }

            if (FD_ISSET(fd, & wfds)) {
                if (!removed) {
                    res++;

                    remove(fd);

                    if (connected)
                        connected(fd);
                }

                --rcounter;
            }
        }
    }

    return res;
}

#endif // NETTY__SELECT_ENABLED

#if NETTY__POLL_ENABLED

template <>
connecting_poller<posix::poll_poller>::connecting_poller (specialized)
    : _rep(POLLERR | POLLHUP | POLLOUT

#ifdef POLLRDHUP
        | POLLRDHUP
#endif

#ifdef POLLWRNORM
        | POLLWRNORM
#endif

#ifdef POLLWRBAND
        | POLLWRBAND
#endif
    )
{}

template <>
int connecting_poller<posix::poll_poller>::poll (std::chrono::milliseconds millis, error * perr)
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
            // Identical to `epoll_poller`
            if (revents & (POLLHUP
#ifdef POLLRDHUP
                | POLLRDHUP
#endif
            )) {
                remove(fd);

                if (connection_refused)
                    connection_refused(fd);

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

                if (connected)
                    connected(fd);
            }
        }
    }

    return res;
}

#endif // NETTY__POLL_ENABLED

#if NETTY__SELECT_ENABLED
template class connecting_poller<posix::select_poller>;
#endif

#if NETTY__POLL_ENABLED
template class connecting_poller<posix::poll_poller>;
#endif

} // namespace netty
