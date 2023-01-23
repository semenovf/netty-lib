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
#include "pfs/netty/linux/epoll_poller.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace netty {
namespace linux_os {

static constexpr std::size_t const DEFAULT_INCREMENT = 32;

epoll_poller::epoll_poller ()
{
    // Since Linux 2.6.8, the size argument is ignored, but must be greater than zero;
    int size = 1024;
    eid = epoll_create(size);

    if (eid < 0) {
        throw error {
              make_error_code(errc::poller_error)
            , tr::_("epoll create failure")
            , pfs::system_error_text()
        };
    }
}

epoll_poller::~epoll_poller ()
{
    if (eid > 0) {
        ::close(eid);
        eid = -1;
    }
}

void epoll_poller::add (native_socket_type sock, error * perr)
{
    struct epoll_event ev;
    ev.events = EPOLLERR | EPOLLHUP | EPOLLRDHUP
        | EPOLLIN | EPOLLRDNORM | EPOLLRDBAND
        | EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND;
    ev.data.fd = sock;

    int rc = epoll_ctl(eid, EPOLL_CTL_ADD, sock, & ev);

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

    if (events.size() % DEFAULT_INCREMENT == 0 )
        events.reserve(events.capacity() + DEFAULT_INCREMENT);

    events.resize(events.size() + 1);
}

void epoll_poller::remove (native_socket_type sock, error * perr)
{
    auto rc = epoll_ctl(eid, EPOLL_CTL_DEL, sock, nullptr);

    if (rc != 0) {
        // ENOENT is not a failure
        if (errno != ENOENT) {
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
}

int epoll_poller::poll (std::chrono::milliseconds millis, error * perr)
{
    auto maxevents = static_cast<int>(events.size());

    if (maxevents == 0)
        return 0;

    auto n = epoll_wait(eid, events.data(), maxevents, millis.count());

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

    return n;
}

bool epoll_poller::empty () const noexcept
{
    return events.size() == 0;
}

}} // namespace netty::linux_os
