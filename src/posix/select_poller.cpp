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
    max_fd = -1;
    min_fd = (std::numeric_limits<native_socket_type>::max)();
}

void select_poller::add (native_socket_type sock, error * perr)
{
    (void)perr;

    if (observe_read)
        FD_SET(sock, & readfds);

    if (observe_write)
        FD_SET(sock, & writefds);

    max_fd = (std::max)(sock, max_fd);
    min_fd = (std::min)(sock, min_fd);
}

void select_poller::remove (native_socket_type sock, error * perr)
{
    (void)perr;

    if (observe_read)
        FD_CLR(sock, & readfds);

    if (observe_write)
        FD_CLR(sock, & writefds);

    if (sock == max_fd)
        --max_fd;

    if (sock == min_fd)
        ++min_fd;
}

int select_poller::poll (fd_set * rfds, fd_set * wfds
    , std::chrono::milliseconds millis, error * perr)
{
    if (max_fd < 0)
        return 0;

    timeval timeout;

    timeout.tv_sec  = millis.count() / 1000;
    timeout.tv_usec = (millis.count() - timeout.tv_sec * 1000) * 1000;

    if (!observe_read)
        rfds = nullptr;

    if (!observe_write)
        wfds = nullptr;

    if (rfds)
        memcpy(rfds, & readfds, sizeof(readfds));

    if (wfds)
        memcpy(wfds, & writefds, sizeof(writefds));

    // Total number of descriptors on success
    auto n = ::select(max_fd + 1, rfds, wfds, nullptr, & timeout);

    if (n < 0) {
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
    return max_fd == 0;
}

}} // namespace netty::posix
