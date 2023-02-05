////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/i18n.hpp"
#include "pfs/netty/posix/select_poller.hpp"
#include <algorithm>

namespace netty {
namespace posix {

select_poller::select_poller (bool oread, bool owrite)
    : observe_read(oread)
    , observe_write(owrite)
{
    FD_ZERO(& readfds);
    FD_ZERO(& writefds);
}

select_poller::~select_poller ()
{
    FD_ZERO(& readfds);
    FD_ZERO(& writefds);
}

void select_poller::add (native_socket_type sock, error * perr)
{
    auto pos = std::find(sockets.begin(), sockets.end(), sock);

    // Already exists
    if (pos != sockets.end())
        return;

    pos = std::find(sockets.begin(), sockets.end(), kINVALID_SOCKET);

    // No free element found, append new socket
    if (pos == sockets.end())
        sockets.push_back(sock);
    else
        *pos = sock;

    ++count;

    (void)perr;

    if (observe_read) {
        if (!FD_ISSET(sock, & readfds)) {
            FD_SET(sock, & readfds);
        }
    }

    if (observe_write) {
        if (!FD_ISSET(sock, & writefds)) {
            FD_SET(sock, & writefds);
        }
    }
}

void select_poller::remove (native_socket_type sock, error * perr)
{
    (void)perr;

    if (observe_read) {
        FD_CLR(sock, & readfds);
    }

    if (observe_write) {
        FD_CLR(sock, & writefds);
    }

    auto pos = std::find(sockets.begin(), sockets.end(), sock);

    // Exists
    if (pos != sockets.end()) {
        *pos = kINVALID_SOCKET;
        --count;
    }
}

int select_poller::poll (fd_set * rfds, fd_set * wfds
    , std::chrono::milliseconds millis, error * perr)
{
    if (sockets.empty())
        return 0;

    timeval timeout;
    bool need_select = false;

    timeout.tv_sec  = millis.count() / 1000;
    timeout.tv_usec = (millis.count() - timeout.tv_sec * 1000) * 1000;

    if (!observe_read)
        rfds = nullptr;

    if (!observe_write)
        wfds = nullptr;

    if (rfds) {
        memcpy(rfds, & readfds, sizeof(readfds));

        if (rfds->fd_count > 0)
            need_select = true;
    }

    if (wfds) {
        memcpy(wfds, & writefds, sizeof(writefds));

        if (wfds->fd_count > 0)
            need_select = true;
    }

    // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select
    // Any two of the parameters, readfds, writefds, or exceptfds, 
    // can be given as null. At least one must be non-null, 
    // and any non-null descriptor set must contain at least one 
    // handle to a socket.
    if (!need_select)
        return 0;
    
    // Total number of descriptors on success
#if _MSC_VER
    // First argument ingored in WinSock2
    auto n = ::select(0, rfds, wfds, nullptr, & timeout);
#else
    auto n = ::select(max_fd + 1, rfds, wfds, nullptr, & timeout);
#endif

#if _MSC_VER
    if (n == SOCKET_ERROR) {
#else
    if (n < 0) {
#endif
        error err {
              errc::poller_error
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

    return n;
}

bool select_poller::empty () const noexcept
{
    return count == 0;
}

}} // namespace netty::posix
