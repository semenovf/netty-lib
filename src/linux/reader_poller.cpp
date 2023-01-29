////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../reader_poller.hpp"

#if NETTY__EPOLL_ENABLED
#   include "pfs/netty/linux/epoll_poller.hpp"
#endif

#include <sys/socket.h>

namespace netty {

#if NETTY__EPOLL_ENABLED

template <>
reader_poller<linux_os::epoll_poller>::reader_poller (specialized)
    : _rep(EPOLLERR | EPOLLIN | EPOLLRDNORM | EPOLLRDBAND)
{}

template <>
int reader_poller<linux_os::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep.poll(millis, perr);

    if (n < 0)
        return n;

    int res = 0;

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            auto revents = _rep.events[i].events;
            auto fd = _rep.events[i].data.fd;

            if (revents & EPOLLERR) {
                int error_val = 0;
                socklen_t len = sizeof(error_val);
                auto rc = getsockopt(fd, SOL_SOCKET, SO_ERROR, & error_val, & len);

                remove(fd);

                if (rc != 0) {
                    on_error(fd, tr::f_("get socket option failure: {}, socket removed: {}"
                        , pfs::system_error_text(), fd));
                } else {
                    on_error(fd, tr::f_("read socket failure: {}, socket removed: {}"
                        , pfs::system_error_text(error_val), fd));
                }

                if (disconnected)
                    disconnected(fd);

                continue;
            }

            if (revents & (EPOLLIN | EPOLLRDNORM | EPOLLRDBAND)) {
                bool disconnect = false;
                char buf[1];
                auto n = ::recv(fd, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);

                if (n > 0) {
                    res++;

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

#endif // #NETTY__EPOLL_ENABLED

#if NETTY__EPOLL_ENABLED
template class reader_poller<linux_os::epoll_poller>;
#endif // NETTY__EPOLL_ENABLED

} // namespace netty

