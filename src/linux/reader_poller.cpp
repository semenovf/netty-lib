////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../reader_poller_impl.hpp"
#include "netty/linux/epoll_poller.hpp"
#include <pfs/i18n.hpp>
#include <sys/socket.h>

NETTY__NAMESPACE_BEGIN

template <>
reader_poller<linux_os::epoll_poller>::reader_poller ()
    : _rep(new linux_os::epoll_poller(EPOLLERR | EPOLLIN | EPOLLRDNORM | EPOLLRDBAND
        | EPOLLHUP | EPOLLRDHUP
    ))
{}

template <>
int reader_poller<linux_os::epoll_poller>::poll (std::chrono::milliseconds millis, error * perr)
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

            if (ev.events & EPOLLERR) {
                int error_val = 0;
                socklen_t len = sizeof(error_val);
                auto rc = getsockopt(ev.data.fd, SOL_SOCKET, SO_ERROR, & error_val, & len);

                if (rc != 0) {
                    on_failure(ev.data.fd, error { make_error_code(pfs::errc::system_error)
                        , tr::f_("get socket ({}) option failure: {} (errno={})"
                            , ev.data.fd, pfs::system_error_text(), errno)
                    });
                } else {
                    if (error_val == EPIPE || error_val == ETIMEDOUT || error_val == EHOSTUNREACH
                            || error_val == ECONNRESET) {
                        on_disconnected(ev.data.fd);
                    } else {
                        on_failure(ev.data.fd, error { make_error_code(pfs::errc::system_error)
                            , tr::f_("get socket ({}) option failure: {} (error_val={})"
                                , ev.data.fd, pfs::system_error_text(error_val), error_val)
                        });
                    }
                }

                continue;
            }

            if (ev.events & (EPOLLHUP | EPOLLRDHUP)) {
                on_disconnected(ev.data.fd);
                continue;
            }

            if (ev.events & (EPOLLIN | EPOLLRDNORM | EPOLLRDBAND)) {
                res++;
                char buf[1];
                auto n = ::recv(ev.data.fd, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);

                if (n > 0) {
                    on_ready_read(ev.data.fd);
                } else if (n == 0) {
                    on_disconnected(ev.data.fd);
                } else {
                    if (errno == ECONNRESET) {
                        on_disconnected(ev.data.fd);
                    } else {
                        on_failure(ev.data.fd, error { make_error_code(pfs::errc::system_error)
                            , tr::f_("read socket failure: {} (socket={})"
                                , pfs::system_error_text(errno), ev.data.fd)
                        });
                    }
                }
            }
        }
    }

    return res;
}

template class reader_poller<linux_os::epoll_poller>;

NETTY__NAMESPACE_END
