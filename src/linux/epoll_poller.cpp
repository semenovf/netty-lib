////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/i18n.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/poller.hpp"
#include "pfs/netty/linux/epoll_poller.hpp"
#include <algorithm>
#include <unistd.h>

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
poller<BACKEND> & poller<BACKEND>::operator = (poller && other)
{
    this->~poller();
    _rep.eid = other._rep.eid;
    other._rep.eid = -1;
    return *this;
}

template <>
poller<BACKEND>::poller (poller && other)
{
    this->operator = (std::move(other));
}

template <>
void poller<BACKEND>::add (native_type sock, error * perr)
{
    struct epoll_event ev;
    ev.events = EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLIN | EPOLLRDNORM
        | EPOLLOUT | EPOLLWRNORM;
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
void poller<BACKEND>::remove (native_type sock, error * perr)
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

            // Error condition (output only).
            //
            // TODO Research this feature and implement handling
            //
            // 1. Occured while tcp socket attempts to connect to non-existance server socket (connection_refused)
            // 2. ... ?
            if (revents & EPOLLERR) {
                on_error(fd);
                continue;
            }

            // Hang up (output only).
            //
            // Contexts:
            // a. Attempt to connect to defunct server address/port
            // b. ...
            if (revents & (EPOLLHUP | EPOLLRDHUP)) {
                disconnected(fd);
                remove(fd);
                continue;
            }

            // There is data to read
            if (revents & (EPOLLIN | EPOLLRDNORM)) {
                ready_read(fd); // May be pending data
            }

            // Writing is now possible, though a write larger than the available space
            // in a socket or pipe will still block (unless O_NONBLOCK is set).
            if (revents & (EPOLLOUT | EPOLLWRNORM)) {
                can_write(fd);
            }

            // EPOLLPRI - There is urgent data to read (e.g., out-of-band data on TCP socket;
            // pseudo-terminal master in packet mode has seen state change in slave).
            // Ignore this events
            if (revents & (EPOLLPRI | EPOLLRDBAND | EPOLLWRBAND | EPOLLMSG
                    | EPOLLEXCLUSIVE | EPOLLWAKEUP | EPOLLONESHOT | EPOLLET))
                unsupported_event(fd, revents);
        }
    }

    return n;
}

template <>
bool poller<BACKEND>::empty () const noexcept
{
    return _rep.events.size() == 0;
}

} // namespace netty
