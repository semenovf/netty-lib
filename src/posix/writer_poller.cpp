////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../writer_poller_impl.hpp"
#include "netty/namespace.hpp"
#include <pfs/i18n.hpp>

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

NETTY__NAMESPACE_BEGIN

#if NETTY__SELECT_ENABLED

template <>
writer_poller<posix::select_poller>::writer_poller ()
    : _rep(new posix::select_poller(false, true))
{}

template <>
int writer_poller<posix::select_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    fd_set wfds;

    auto n = _rep->poll(nullptr, & wfds, millis, perr);

    if (n < 0)
        return n;

    auto res = 0;

    if (n > 0) {
        int rcounter = n;

        auto pos  = _rep->sockets.begin();
        auto last = _rep->sockets.end();

        for (; pos != last && rcounter > 0; ++pos) {
            auto fd = *pos;

            if (fd == posix::select_poller::kINVALID_SOCKET)
                continue;

            if (FD_ISSET(fd, & wfds)) {
                res++;
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
writer_poller<posix::poll_poller>::writer_poller ()
    : _rep(new posix::poll_poller(POLLERR | POLLOUT

#ifdef POLLWRNORM
        | POLLWRNORM
#endif

#ifdef POLLWRBAND
        | POLLWRBAND
#endif
    ))
{}

template <>
int writer_poller<posix::poll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep->poll(millis, perr);

    if (n < 0)
        return n;

    auto res = 0;

    if (n > 0) {
        for (auto const & ev: _rep->events) {
            if (n == 0)
                break;

            if (ev.revents == 0)
                continue;

            n--;

            // This event is also reported for the write end of a pipe when
            // the read end has been closed.
            // TODO Recognize disconnection (EPIPE ?)
            if (ev.revents & POLLERR) {
                int error_val = 0;
                socklen_t len = sizeof(error_val);
                auto rc = getsockopt(ev.fd, SOL_SOCKET, SO_ERROR, & error_val, & len);

                if (rc != 0) {
                    on_failure(ev.fd, error {
                          make_error_code(pfs::errc::system_error)
                        , tr::f_("get socket option failure: {} (socket={})"
                            , pfs::system_error_text(), ev.fd)
                    });
                } else {
                    if (error_val == ECONNRESET) {
                        on_disconnected(ev.fd);
                    } else {
                        on_failure(ev.fd, error {
                            errc::socket_error
                            , tr::f_("write socket failure: {} (socket={})"
                                , pfs::system_error_text(error_val), ev.fd)
                        });
                    }
                }

                continue;
            }

            // Writing is now possible, though a write larger than the available space
            // in a socket or pipe will still block (unless O_NONBLOCK is set).
            // Identical to `epoll_poller`
            if (ev.revents & (POLLOUT
#ifdef POLLWRNORM
                | POLLWRNORM
#endif
#ifdef POLLWRBAND
                | POLLWRBAND
#endif
            )) {
                res++;
                can_write(ev.fd);
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

NETTY__NAMESPACE_END
