////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/namespace.hpp"
#include "netty/error.hpp"
#include "netty/linux/epoll_poller.hpp"
#include <pfs/i18n.hpp>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

NETTY__NAMESPACE_BEGIN

namespace linux_os {

static constexpr std::size_t const DEFAULT_INCREMENT = 32;

epoll_poller::epoll_poller (std::uint32_t observable_events)
    : oevents(observable_events)
{
    // Since Linux 2.6.8, the size argument is ignored, but must be greater than zero;
    int size = 1024;
    eid = epoll_create(size);

    if (eid < 0) {
        throw error {tr::f_("epoll create failure: {}", pfs::system_error_text())};
    }
}

epoll_poller::~epoll_poller ()
{
    if (eid > 0) {
        ::close(eid);
        eid = -1;
    }
}

void epoll_poller::add_socket (socket_id sock, error * perr)
{
    struct epoll_event ev;
    ev.events = oevents;
    ev.data.fd = sock;

    int rc = epoll_ctl(eid, EPOLL_CTL_ADD, sock, & ev);

    if (rc != 0) {
        // Is not an error
        if (errno == EEXIST)
            return;

        pfs::throw_or(perr, error {
            tr::f_("epoll add socket ({}) failure: }", sock, pfs::system_error_text())
        });

        return;
    }

    if (events.size() % DEFAULT_INCREMENT == 0 )
        events.reserve(events.capacity() + DEFAULT_INCREMENT);

    events.resize(events.size() + 1);
}

void epoll_poller::add_listener (listener_id sock, error * perr)
{
    add_socket(sock);
}

void epoll_poller::wait_for_write (socket_id sock, error * perr)
{
    add_socket(sock, perr);
}

void epoll_poller::remove_socket (socket_id sock, error * perr)
{
    auto rc = epoll_ctl(eid, EPOLL_CTL_DEL, sock, nullptr);

    if (rc != 0) {
        // ENOENT is not a failure
        if (errno != ENOENT) {
            pfs::throw_or(perr, error {tr::f_("epoll delete failure: {}", pfs::system_error_text())});
            return;
        }
    }
}

void epoll_poller::remove_listener (listener_id sock, error * perr)
{
    remove_socket(sock);
}

int epoll_poller::poll (std::chrono::milliseconds millis, error * perr)
{
    auto maxevents = static_cast<int>(events.size());

    if (maxevents == 0)
        return 0;

    if (millis < std::chrono::milliseconds{0})
        millis = std::chrono::milliseconds{0};

    auto n = epoll_wait(eid, events.data(), maxevents, millis.count());

    if (n < 0) {
        if (errno == EINTR) {
            // Is not a critical error, ignore it
        } else {
            pfs::throw_or(perr, error {tr::f_("epoll wait failure: {}", pfs::system_error_text())});
            return n;
        }
    }

    return n;
}

bool epoll_poller::empty () const noexcept
{
    return events.size() == 0;
}

} // namespace linux_os

NETTY__NAMESPACE_END
