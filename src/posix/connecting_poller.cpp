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

    if (n > 0) {
        int rcounter = n;

        auto pos  = _rep.sockets.begin();
        auto last = _rep.sockets.end();

        for (; pos != last && rcounter > 0; ++pos) {
            auto fd = *pos;

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
                    on_failure(fd, error {
                          errc::system_error
                        , tr::f_("get socket option failure: {} (socket={})"
                            , pfs::system_error_text(), fd)
                    });
                } else {
                    switch (error_val) {
                        case 0: // No error
                            break;

                        case ECONNREFUSED:
                            connection_refused(fd);
                            break;

                        default:
                            on_failure(fd, error {
                                  errc::unexpected_error
                                , tr::f_("unhandled error value returned by `getsockopt`: {} (socket={})"
                                    , error_val, fd)
                            });
                            break;
                    }
                }

                --rcounter;
            }

            if (FD_ISSET(fd, & wfds)) {
                connected(fd);
                --rcounter;
            }
        }
    }

    return n;
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
        for (auto const & ev: _rep.events) {
            if (n == 0)
                break;

            if (ev.revents == 0)
                continue;

            n--;

            // 1. Occured while tcp socket attempts to connect to non-existance server socket (connection_refused)
            // 2. No route to host
            // 3. ... ?
            if (ev.revents & POLLERR) {
                int error_val = 0;
                socklen_t len = sizeof(error_val);
                auto rc = getsockopt(ev.fd, SOL_SOCKET, SO_ERROR, & error_val, & len);

                if (rc != 0) {
                    on_failure(ev.fd, error {
                          errc::system_error
                        , tr::f_("get socket option failure: {} (socket={})"
                            , pfs::system_error_text(), ev.fd)
                    });
                } else {
                    switch (error_val) {
                        case 0: // No error
                            on_failure(ev.fd, error {
                                  errc::unexpected_error
                                , tr::f_("EPOLLERR event happend, but no error occurred on it (socket={})"
                                    , ev.fd)
                            });
                            break;

                        case EHOSTUNREACH:
                            on_failure(ev.fd, error {
                                  errc::socket_error
                                , tr::f_("no route to host (socket={})", ev.fd)
                            });
                            break;

                        case ECONNREFUSED:
                            connection_refused(ev.fd);
                            break;

                        default:
                            on_failure(ev.fd, error {
                                  errc::unexpected_error
                                , tr::f_("unhandled error value returned by `getsockopt`: {} (socket={})"
                                    , error_val, ev.fd)
                            });
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
            if (ev.revents & (POLLHUP
#ifdef POLLRDHUP
                | POLLRDHUP
#endif
            )) {
                connection_refused(ev.fd);
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
                connected(ev.fd);
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
