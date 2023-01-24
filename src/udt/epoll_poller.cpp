////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "newlib/udt.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/udt/epoll_poller.hpp"
#include <set>

#include "pfs/log.hpp"

static char const * TAG = "UDT";

namespace netty {
namespace udt {

epoll_poller::epoll_poller (bool oread, bool owrite)
    : observe_read(oread)
    , observe_write(owrite)
{
    eid = UDT::epoll_create();

    if (eid < 0) {
        throw error {
              make_error_code(errc::poller_error)
            , tr::_("UDT epoll_poller: create failure")
            , UDT::getlasterror_desc()
        };
    }
}

epoll_poller::~epoll_poller ()
{
    if (eid != UDT::ERROR) {
        UDT::epoll_release(eid);
        eid = UDT::ERROR;
    }
}

void epoll_poller::add (native_socket_type sock, error * perr)
{
    int events = UDT_EPOLL_ERR;

    if (observe_read)
        events |= UDT_EPOLL_IN;

    if (observe_write)
        events |= UDT_EPOLL_OUT;

    auto rc = UDT::epoll_add_usock(eid, sock, & events);

    if (rc == UDT::ERROR) {
        error err {
              make_error_code(errc::poller_error)
            , tr::_("UDT epoll_poller: add socket failure")
            , UDT::getlasterror_desc()
        };

        if (perr) {
            *perr = std::move(err);
            return;
        } else {
            throw err;
        }
    }

    LOGD(TAG, "ADD SOCKET: eid={}, sock={}", eid, sock);

    ++counter;
}

void epoll_poller::remove (native_socket_type sock, error * perr)
{
    auto rc = UDT::epoll_remove_usock(eid, sock);

    LOGD(TAG, "REMOVE SOCKET: eid={}, sock={}", eid, sock);

    if (rc == UDT::ERROR) {
        error err {
              make_error_code(errc::poller_error)
            , tr::_("UDT epoll_poller: delete failure")
            , UDT::getlasterror_desc()
        };

        if (perr) {
            *perr = std::move(err);
            return;
        } else {
            throw err;
        }
    }

    --counter;

    if (counter < 0) {
        error err {
              make_error_code(errc::poller_error)
            , tr::_("UDT epoll_poller: counter management not consistent")
            , UDT::getlasterror_desc()
        };

        throw err;
    }
}

bool epoll_poller::empty () const noexcept
{
    return counter == 0;
}

int epoll_poller::poll (int eid, std::set<UDTSOCKET> * readfds
    , std::set<UDTSOCKET> * writefds, std::chrono::milliseconds millis, error * perr)
{
    if (!observe_read)
        readfds = nullptr;

    if (!observe_write)
        writefds = nullptr;

    auto n = UDT::epoll_wait(eid, readfds, writefds, millis.count(), nullptr, nullptr);

    if (n < 0) {
        auto ec = UDT::getlasterror().getErrorCode();

        if (ec == UDT::ERRORINFO::ETIMEOUT) {
            n = 0;
        } else {
            error err {
                make_error_code(errc::poller_error)
                , tr::_("UDT epoll_poller: poll failure")
                , UDT::getlasterror_desc()
            };

            if (perr) {
                *perr = std::move(err);
                return n;
            } else {
                throw err;
            }
        }
    }

    return n;
}

}}  // namespace netty::udt

