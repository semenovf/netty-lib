////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../reader_poller_impl.hpp"

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

NETTY__NAMESPACE_BEGIN

#if NETTY__SELECT_ENABLED

template <>
reader_poller<posix::select_poller>::reader_poller ()
    : _rep(new posix::select_poller(true, false))
{}

template <>
int reader_poller<posix::select_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    fd_set rfds;

    auto n = _rep->poll(& rfds, nullptr, millis, perr);

    if (n < 0)
        return n;

    auto res = 0;

    if (n > 0) {
        int rcounter = n;

        auto pos  = _rep->sockets.begin();
        auto last = _rep->sockets.end();

        for (; pos != last && rcounter > 0; ++pos) {
            auto fd = *pos;

            if (fd == posix::select_poller::kINVALID_SOCKET)
                continue;

            if (FD_ISSET(fd, & rfds)) {
                res++;

                char buf[1];
#if _MSC_VER
                auto n1 = ::recv(fd, buf, sizeof(buf), MSG_PEEK);
#else
                auto n1 = ::recv(fd, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);
#endif

                if (n1 > 0) {
                    on_ready_read(fd);
                } else if (n1 == 0) {
                    on_disconnected(fd);
                } else {
#if _MSC_VER
                    auto lastWsaError = WSAGetLastError();

                    // The message was too large to fit into the specified buffer and was truncated.
                    // Is not an error. Process this error as normal result.
                    if (lastWsaError == WSAEMSGSIZE) {
                        on_ready_read(fd);
                    } else {
#endif
                        if (errno == ECONNRESET) {
                            on_disconnected(fd);
                        } else {
                            on_failure(fd, error {
                                tr::f_("read socket failure: {} (socket={})", pfs::system_error_text(errno), fd)
                            });
                        }
#if _MSC_VER
                    }
#endif
                }

                --rcounter;
            }
        }
    }

    return res;
}

#endif // NETTY__SELECT_ENABLED

#if NETTY__POLL_ENABLED

template <>
reader_poller<posix::poll_poller>::reader_poller ()
    : _rep(new posix::poll_poller(POLLERR | POLLIN | POLLNVAL

#ifdef POLLRDNORM
        | POLLRDNORM
#endif

#ifdef POLLRDBAND
        | POLLRDBAND
#endif
    ))
{}

template <>
int reader_poller<posix::poll_poller>::poll (std::chrono::milliseconds millis, error * perr)
{
    auto n = _rep->poll(millis, perr);

    if (n < 0)
        return n;

    auto res = 0;

    if (n > 0) {
        for (auto const & ev: _rep->events) {
            if (n == 0)
                break;

            if (ev.revents == 0)
                continue;

            n--;

            if (ev.revents & POLLERR) {
                int error_val = 0;
                socklen_t len = sizeof(error_val);
                auto rc = getsockopt(ev.fd, SOL_SOCKET, SO_ERROR, & error_val, & len);

                if (rc != 0) {
                    on_failure(ev.fd, error {
                        tr::f_("get socket option failure: {} (socket={})"
                            , pfs::system_error_text(), ev.fd)
                    });
                } else {
                    if (error_val == EPIPE || error_val == ETIMEDOUT || error_val == EHOSTUNREACH
                            || error_val == ECONNRESET) {
                        on_disconnected(ev.fd);
                    } else {
                        on_failure(ev.fd, error {
                            tr::f_("get socket ({}) option failure: {} (error_val={})"
                                , ev.fd, pfs::system_error_text(error_val), error_val)
                        });
                    }
                }

                continue;
            }

            // There is data to read
            if (ev.revents & (POLLIN
#ifdef POLLRDNORM
                | POLLRDNORM
#endif
#ifdef POLLRDBAND
                | POLLRDBAND
#endif
            )) {
                res++;

                char buf[1];
                auto n = ::recv(ev.fd, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);

                if (n > 0) {
                    on_ready_read(ev.fd);
                } else if (n == 0) {
                    on_disconnected(ev.fd);
                } else {
                    if (errno != ECONNRESET) {
                        on_failure(ev.fd, error {
                            tr::f_("read socket failure: {} (socket={})"
                                , pfs::system_error_text(errno), ev.fd)
                        });
                    } else {
                        on_disconnected(ev.fd);
                    }
                }
            }
        }
    }

    return res;
}

#endif // NETTY__POLL_ENABLED

#if NETTY__SELECT_ENABLED
template class reader_poller<posix::select_poller>;
#endif

#if NETTY__POLL_ENABLED
template class reader_poller<posix::poll_poller>;
#endif

NETTY__NAMESPACE_END
