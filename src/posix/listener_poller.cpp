////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../listener_poller.hpp"

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

namespace netty {

#if NETTY__SELECT_ENABLED

template <>
listener_poller<posix::select_poller>::listener_poller (std::shared_ptr<posix::select_poller> ptr)
    : _rep(ptr == nullptr
        ? std::make_shared<posix::select_poller>(true, false)
        : std::move(ptr))
{
    init();
}

template <>
int listener_poller<posix::select_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    fd_set rfds;

    auto n = _rep->poll(& rfds, nullptr, millis, perr);

    if (n < 0)
        return n;

    int res = 0;

    if (n > 0) {
        int rcounter = n;

        auto pos  = _rep->sockets.begin();
        auto last = _rep->sockets.end();

        for (; pos != last && rcounter > 0; ++pos) {
            auto fd = *pos;

            if (FD_ISSET(fd, & rfds)) {
                res++;
                accept(fd);
                --rcounter;
            }
        }
    }

    return res;
}

#endif // NETTY__SELECT_ENABLED

#if NETTY__POLL_ENABLED

template <>
listener_poller<posix::poll_poller>::listener_poller (std::shared_ptr<posix::poll_poller> ptr)
    : _rep(ptr == nullptr ? std::make_shared<posix::poll_poller>(POLLERR | POLLIN

#ifdef POLLRDNORM
        | POLLRDNORM
#endif

#ifdef POLLRDBAND
        | POLLRDBAND
#endif
    ) : std::move(ptr))
{
    init();
}

template <>
int listener_poller<posix::poll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep->poll(millis, perr);

    if (n < 0)
        return n;

    int res = 0;

    if (n > 0) {
        for (auto const & ev: _rep->events) {
            if (n == 0)
                break;

            if (ev.revents == 0)
                continue;

            n--;

            // 1. Occured while tcp socket attempts to connect to non-existance server socket (connection_refused)
            // 2. ... ?
            if (ev.revents & POLLERR) {
                int error_val = 0;
                socklen_t len = sizeof(error_val);
                auto rc = getsockopt(ev.fd, SOL_SOCKET, SO_ERROR, & error_val, & len);

                if (rc != 0) {
                    on_failure(ev.fd, error {
                          make_error_code(pfs::errc::system_error)
                        , tr::f_("get socket option failure: {}, socket removed: {}"
                            , pfs::system_error_text(), ev.fd)
                    });
                } else {
                    on_failure(ev.fd, error {
                          errc::socket_error
                        , tr::f_("accept socket error: {}, socket removed: {}"
                            , pfs::system_error_text(error_val), ev.fd)
                    });
                }

                continue;
            }

            // There is data to read - can accept
            if (ev.revents & (POLLIN
#ifdef POLLRDNORM
                | POLLRDNORM
#endif
#ifdef POLLRDBAND
                | POLLRDBAND
#endif
            )) {
                res++;
                accept(ev.fd);
            }
        }
    }

    return res;
}

#endif // NETTY__POLL_ENABLED

#if NETTY__SELECT_ENABLED
template class listener_poller<posix::select_poller>;
#endif

#if NETTY__POLL_ENABLED
template class listener_poller<posix::poll_poller>;
#endif

} // namespace netty
