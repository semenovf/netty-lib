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
connecting_poller<linux_os::epoll_poller>::connecting_poller (std::shared_ptr<linux_os::epoll_poller>)
    : _rep(std::make_shared<linux_os::epoll_poller>(EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND))
{
    init();
}

template <>
int connecting_poller<linux_os::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep->poll(millis, perr);

    if (n < 0)
        return n;

    int res = 0;

    if (n > 0) {
        for (auto const & ev: _rep->events) {
            if (n == 0)
                break;

            if (ev.events == 0)
                continue;

            n--;

            // 1. Occured while tcp socket attempts to connect to non-existance server socket (connection_refused)
            // 2. No route to host
            // 3. ... ?
            if (ev.events & EPOLLERR) {
                int error_val = 0;
                socklen_t len = sizeof(error_val);
                auto rc = getsockopt(ev.data.fd, SOL_SOCKET, SO_ERROR, & error_val, & len);

                if (rc != 0) {
                    on_failure(ev.data.fd
                        , error {
                              make_error_code(pfs::errc::system_error)
                            , tr::f_("get socket option failure: {} (socket={})"
                                , pfs::system_error_text(), ev.data.fd)
                        });
                } else {
                    switch (error_val) {
                        case 0: // No error
                            on_failure(ev.data.fd, error {
                                  make_error_code(pfs::errc::unexpected_error)
                                , tr::f_("EPOLLERR event happend, but no error occurred on it (socket={})"
                                , ev.data.fd)
                            });
                            break;

                        case EHOSTUNREACH:
                            on_failure(ev.data.fd, error {
                                  errc::socket_error
                                , tr::f_("no route to host (socket={})", ev.data.fd)
                            });
                            break;

                        case ECONNREFUSED:
                            connection_refused(ev.data.fd);
                            break;

                        default:
                            on_failure(ev.data.fd, error {
                                  make_error_code(pfs::errc::unexpected_error)
                                , tr::f_("unhandled error value returned by `getsockopt`: {} (socket={})"
                                    , error_val, ev.data.fd)
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
            if (ev.events & (EPOLLHUP | EPOLLRDHUP)) {
                connection_refused(ev.data.fd);
                continue;
            }

            // Writing is now possible, though a write larger than the available space
            // in a socket or pipe will still block (unless O_NONBLOCK is set).
            if (ev.events & (EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND)) {
                res++;
                connected(ev.data.fd);
            }
        }
    }

    return res;
}

#endif // #NETTY__EPOLL_ENABLED

#if NETTY__EPOLL_ENABLED
template class connecting_poller<linux_os::epoll_poller>;
#endif // NETTY__EPOLL_ENABLED

} // namespace netty
