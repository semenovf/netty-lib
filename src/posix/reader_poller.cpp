////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../reader_poller.hpp"

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

#include "pfs/log.hpp"

static char const * TAG = "POSIX";

namespace netty {

#if NETTY__SELECT_ENABLED

template <>
reader_poller<posix::select_poller>::reader_poller (specialized)
    : _rep(true, false)
{}

template <>
int reader_poller<posix::select_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    fd_set rfds;

    auto n = _rep.poll(& rfds, nullptr, millis, perr);

    if (n < 0)
        return n;

    auto res = 0;

    if (n > 0) {
        int rcounter = n;

        for (int fd = _rep.min_fd; fd <= _rep.max_fd && rcounter > 0; fd++) {
            if (FD_ISSET(fd, & rfds)) {
                res++;

                bool disconnect = false;
                char buf[1];
#if _MSC_VER
                auto n1 = ::recv(fd, buf, sizeof(buf), MSG_PEEK);
#else
                auto n1 = ::recv(fd, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);
#endif

                if (n1 > 0) {
                    if (ready_read)
                        ready_read(fd);
                } else if (n1 == 0) {
                    disconnect = true;
                } else {
                    if (errno != ECONNRESET) {
                        on_error(fd, tr::f_("read socket failure: {}"
                            , pfs::system_error_text(errno)));
                    }
                    disconnect = true;
                }

                if (disconnect) {
                    remove(fd);

                    if (disconnected)
                        disconnected(fd);
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
reader_poller<posix::poll_poller>::reader_poller (specialized)
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
int reader_poller<posix::poll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(millis, perr);

    if (n < 0)
        return n;

    auto res = 0;

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            auto revents = _rep.events[i].revents;
            auto fd = _rep.events[i].fd;

            if (revents & POLLERR) {
                LOGD(TAG, "POLL POLLER ERROR");
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

                // Same as for select_poller
                bool disconnect = false;
                char buf[1];
                auto n = ::recv(fd, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);

                if (n > 0) {
                    if (ready_read)
                        ready_read(fd);
                } else if ( n == 0) {
                    disconnect = true;
                } else {
                    if (errno != ECONNRESET) {
                        on_error(fd, tr::f_("read socket failure: {}"
                            , pfs::system_error_text(errno)));
                    }
                    disconnect = true;
                }

                if (disconnect) {
                    remove(fd);

                    if (disconnected)
                        disconnected(fd);
                }
            }
        }
    }

    return res;
}

#endif // NETTY__POLL_ENABLED

#if NETTY__SELECT_ENABLED
template class reader_poller<posix::select_poller>;
#endif

#if NETTY__POLL_ENABLED
template class reader_poller<posix::poll_poller>;
#endif

} // namespace netty
