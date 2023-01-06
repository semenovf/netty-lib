////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../client_poller.hpp"
#include "../server_poller.hpp"
#include "pfs/assert.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/poller.hpp"
#include "pfs/netty/linux/epoll_poller.hpp"
#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace netty {

static constexpr std::size_t const DEFAULT_INCREMENT = 32;
using BACKEND = linux_ns::epoll_poller;

template <>
poller<BACKEND>::poller ()
{
    // Since Linux 2.6.8, the size argument is ignored, but must be greater than zero;
    int size = 1024;
    _rep.eid = epoll_create(size);

    if (_rep.eid < 0) {
        throw error {
              make_error_code(errc::poller_error)
            , tr::_("epoll create failure")
            , pfs::system_error_text()
        };
    }
}

template <>
poller<BACKEND>::~poller ()
{
    if (_rep.eid > 0) {
        ::close(_rep.eid);
        _rep.eid = -1;
    }
}

template <>
void poller<BACKEND>::add (native_socket_type sock, error * perr)
{
    struct epoll_event ev;
    ev.events = EPOLLERR | EPOLLHUP | EPOLLRDHUP
        | EPOLLIN | EPOLLRDNORM | EPOLLRDBAND
        | EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND;
    ev.data.fd = sock;

    int rc = epoll_ctl(_rep.eid, EPOLL_CTL_ADD, sock, & ev);

    if (rc != 0) {
        error err {
              make_error_code(errc::poller_error)
            , tr::_("epoll add socket failure")
            , pfs::system_error_text()
        };

        if (perr) {
            *perr = std::move(err);
            return;
        } else {
            throw err;
        }
    }

    if (_rep.events.size() % DEFAULT_INCREMENT == 0 )
        _rep.events.reserve(_rep.events.capacity() + DEFAULT_INCREMENT);

    _rep.events.resize(_rep.events.size() + 1);
}

template <>
void poller<BACKEND>::remove (native_socket_type sock, error * perr)
{
    auto rc = epoll_ctl(_rep.eid, EPOLL_CTL_DEL, sock, nullptr);

    if (rc != 0) {
        error err {
              make_error_code(errc::poller_error)
            , tr::_("epoll delete failure")
            , pfs::system_error_text()
        };

        if (perr) {
            *perr = std::move(err);
            return;
        } else {
            throw err;
        }
    }
}

template <>
int poller<BACKEND>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto maxevents = static_cast<int>(_rep.events.size());

    if (maxevents == 0)
        return 0;

    auto n = epoll_wait(_rep.eid, _rep.events.data(), maxevents, millis.count());

    if (n < 0) {
        error err {
              make_error_code(errc::poller_error)
            , tr::_("epoll wait failure")
            , pfs::system_error_text()
        };

        if (perr) {
            *perr = std::move(err);
            return n;
        } else {
            throw err;
        }
    }

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
                    if (error_val == 0) {
                        on_error(fd, tr::f_("there was some error in the socket: socket={}"
                            , pfs::system_error_text()));
                    } else {
                        switch (error_val) {
                            case ECONNREFUSED:
                                remove(fd);
                                connection_refused(fd);
                                break;
                            default:
                                on_error(fd, tr::_("unhandled error value returned by `getsockopt`"));
                                break;
                        }
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
            // For compatibility with `client_poller` handle this event  by
            // `ready_read` callback
            // Identical to `poll_poller`
            if (revents & (EPOLLHUP | EPOLLRDHUP)) {
                PFS__ASSERT(ready_read != nullptr, "ready_read must be set for poll");
                // disconnected(fd);
                // remove(fd);
                ready_read(fd, ready_read_flag::disconnected);
                continue;
            }

            // There is data to read
            // Identical to `poll_poller`
            if (revents & (EPOLLIN | EPOLLRDNORM | EPOLLRDBAND)) {
                PFS__ASSERT(ready_read != nullptr, "ready_read must be set for poll");
                ready_read(fd, ready_read_flag::good); // May be pending data
            }

            // Writing is now possible, though a write larger than the available space
            // in a socket or pipe will still block (unless O_NONBLOCK is set).
            // Identical to `poll_poller`
            if (revents & (EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND)) {
                if (can_write)
                    can_write(fd);
            }

            // These events are ignored.
            //  * EPOLLPRI
            //  * EPOLLMSG
            //  * EPOLLEXCLUSIVE
            //  * EPOLLWAKEUP
            //  * EPOLLONESHOT
            //  * EPOLLET
        }
    }

    return n;
}

template <>
bool poller<BACKEND>::empty () const noexcept
{
    return _rep.events.size() == 0;
}

template class server_poller<BACKEND>;
template class client_poller<BACKEND>;

} // namespace netty
