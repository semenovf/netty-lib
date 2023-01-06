////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../client_poller.hpp"
#include "../server_poller.hpp"
#include "pfs/assert.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/poller.hpp"
#include "pfs/netty/posix/poll_poller.hpp"
#include <algorithm>
#include <string.h>

namespace netty {

// static constexpr std::size_t const DEFAULT_INCREMENT = 32;
using BACKEND = posix::poll_poller;

template <>
poller<BACKEND>::poller ()
{}

template <>
poller<BACKEND>::~poller ()
{}

template <>
void poller<BACKEND>::add (native_socket_type sock, error * perr)
{
    (void)perr;

    _rep.events.push_back(pollfd{});
    auto & ev = _rep.events.back();
    ev.fd = sock;
    ev.revents = 0;
    ev.events = POLLERR | POLLHUP | POLLIN | POLLOUT;

#ifdef POLLRDHUP
    ev.events |= POLLRDHUP;
#endif

#ifdef POLLRDNORM
    ev.events |= POLLRDNORM;
#endif

#ifdef POLLRDBAND
    ev.events |= POLLRDBAND;
#endif

#ifdef POLLWRNORM
    ev.events |= POLLWRNORM;
#endif

#ifdef POLLWRBAND
    ev.events |= POLLWRBAND;
#endif
}

template <>
void poller<BACKEND>::remove (native_socket_type sock, error * perr)
{
    (void)perr;

    auto pos = std::find_if(_rep.events.begin(), _rep.events.end()
        , [& sock] (pollfd const & p) { return sock == p.fd;});

    if (pos != _rep.events.end()) {
        // pollfd is POD, so we can simply shift tail to the left and do resize
        // instead of call `erase` method
        auto index = std::distance(_rep.events.begin(), pos);
        auto ptr = _rep.events.data();
        auto sz = _rep.events.size();
        auto n = sz - 1 - index;

        memcpy(ptr + index, ptr + index + 1, n);
        _rep.events.resize(sz - 1);
    }
}

template <>
int poller<BACKEND>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = ::poll(_rep.events.data(), _rep.events.size(), millis.count());

    if (n < 0) {
        error err {
              make_error_code(errc::poller_error)
            , tr::_("poll failure")
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
            // Identical to `epoll_poller`
            if (revents & (POLLHUP
#ifdef POLLRDHUP
                | POLLRDHUP
#endif
            )) {
                PFS__ASSERT(ready_read != nullptr, "ready_read must be set for poll");
                // disconnected(fd);
                // remove(fd);
                ready_read(fd, ready_read_flag::disconnected);
                continue;
            }

            // There is data to read
            // Identical to `epoll_poller`
            if (revents & (POLLIN
#ifdef POLLRDNORM
                | POLLRDNORM
#endif
#ifdef POLLRDBAND
                | POLLRDBAND
#endif
            )) {
                PFS__ASSERT(ready_read != nullptr, "ready_read must be set for poll");
                ready_read(fd, ready_read_flag::good); // May be pending data
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
                if (can_write)
                    can_write(fd);
            }

            // These events are ignored.
            //  * POLLPRI
            //  * POLLET
            //  * POLLNVAL
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

