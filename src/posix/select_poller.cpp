////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../client_poller.hpp"
#include "../server_poller.hpp"
#include "pfs/assert.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/poller.hpp"
#include "pfs/netty/posix/select_poller.hpp"
#include <algorithm>

namespace netty {

using BACKEND = posix::select_poller;

template <>
poller<BACKEND>::poller ()
{
    FD_ZERO(& _rep.readfds);
    FD_ZERO(& _rep.writefds);
}

template <>
poller<BACKEND>::~poller ()
{
    FD_ZERO(& _rep.readfds);
    FD_ZERO(& _rep.writefds);
    _rep.max_fd = -1;
    _rep.min_fd = (std::numeric_limits<native_socket_type>::max)();
}

template <>
void poller<BACKEND>::add (native_socket_type sock, error * perr)
{
    (void)perr;

    FD_SET(sock, & _rep.readfds);
    FD_SET(sock, & _rep.writefds);

    _rep.max_fd = (std::max)(sock, _rep.max_fd);
    _rep.min_fd = (std::min)(sock, _rep.min_fd);
}

template <>
void poller<BACKEND>::remove (native_socket_type sock, error * perr)
{
    (void)perr;

    FD_CLR(sock, & _rep.readfds);
    FD_CLR(sock, & _rep.writefds);

    if (sock == _rep.max_fd)
        --_rep.max_fd;

    if (sock == _rep.min_fd)
        ++_rep.min_fd;
}

template <>
int poller<BACKEND>::poll (std::chrono::milliseconds millis, error * perr)
{
    if (_rep.max_fd < 0)
        return 0;

    timeval timeout;

    timeout.tv_sec  = millis.count() / 1000;
    timeout.tv_usec = (millis.count() - timeout.tv_sec * 1000) * 1000;

    fd_set readfds;
    fd_set writefds;

    memcpy(& readfds, & _rep.readfds, sizeof(_rep.readfds));
    memcpy(& writefds, & _rep.writefds, sizeof(_rep.writefds));

    // Total number of descriptors on success
    auto n = ::select(_rep.max_fd + 1, & readfds, & writefds, nullptr, & timeout);

    if (n < 0) {
        error err {
              make_error_code(errc::poller_error)
            , tr::_("select failure")
            , pfs::system_error_text()
        };

        if (perr) {
            *perr = std::move(err);
            return n;
        } else {
            throw err;
        }
    }

    if (n == 0)
        return 0;

    auto total = n;

    for (int fd = _rep.min_fd; fd <= _rep.max_fd && total > 0; fd++) {
        bool removed = false;

        if (FD_ISSET(fd, & readfds)) {
            int error_val = 0;
            socklen_t len = sizeof(error_val);
            auto rc = getsockopt(fd, SOL_SOCKET, SO_ERROR, & error_val, & len);

            if (rc != 0) {
                remove(fd);
                removed = true;
                on_error(fd, tr::f_("get socket option failure: {}, socket removed"
                    , pfs::system_error_text()));
            } else {
                if (error_val == 0) {
                    PFS__ASSERT(ready_read != nullptr, "ready_read must be set for poll");
                    ready_read(fd, ready_read_flag::check_disconnected);
                } else {
                    switch (error_val) {
                        case ECONNREFUSED:
                            remove(fd);
                            removed = true;
                            connection_refused(fd);
                            break;
                        default:
                            on_error(fd, tr::_("unhandled error value returned by `getsockopt`"));
                            break;
                    }
                }
            }

            --total;
        }

        if (FD_ISSET(fd, & writefds)) {
            if (!removed) {
                if (can_write)
                    can_write(fd);
            }

            --total;
        }
    }

    return n;
}

template <>
bool poller<BACKEND>::empty () const noexcept
{
    return _rep.max_fd == 0;
}

template class server_poller<BACKEND>;
template class client_poller<BACKEND>;

} // namespace netty
