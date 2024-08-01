////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
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

namespace netty {
namespace udt {

epoll_poller::epoll_poller (bool oread, bool owrite)
    : observe_read(oread)
    , observe_write(owrite)
{
    eid = UDT::epoll_create();

    if (eid < 0) {
        throw error {
              errc::poller_error
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

void epoll_poller::add_socket (native_socket_type sock, error * perr)
{
    int events = UDT_EPOLL_ERR;

    if (observe_read)
        events |= UDT_EPOLL_IN;

    if (observe_write)
        events |= UDT_EPOLL_OUT;

    auto rc = UDT::epoll_add_usock(eid, sock, & events);

    if (rc == UDT::ERROR) {
        pfs::throw_or(perr, error {
              errc::poller_error
            , tr::f_("UDT epoll_poller: add socket ({}) failure", sock)
            , UDT::getlasterror_desc()
        });

        return;
    }

    ++counter;
}

void epoll_poller::add_listener (native_listener_type sock, error * perr)
{
    add_socket(sock);
}

void epoll_poller::wait_for_write (native_socket_type sock, error * perr)
{
    add_socket(sock, perr);
}

void epoll_poller::remove_socket (native_socket_type sock, error * perr)
{
    if (counter == 0)
        return;

    auto rc = UDT::epoll_remove_usock(eid, sock);

    if (rc == UDT::ERROR) {
        pfs::throw_or(perr, error {
              errc::poller_error
            , tr::_("UDT epoll_poller: delete failure")
            , UDT::getlasterror_desc()
        });

        return;
    }

    --counter;

    if (counter < 0) {
        pfs::throw_or(perr, error {
              errc::poller_error
            , tr::_("UDT epoll_poller: counter management not consistent")
        });
    }
}

void epoll_poller::remove_listener (native_listener_type sock, error * perr)
{
    remove_socket(sock);
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
            pfs::throw_or(perr, error {
                  errc::poller_error
                , tr::_("UDT epoll_poller: poll failure")
                , UDT::getlasterror_desc()
            });

            return n;
        }
    }

    return n;
}

}}  // namespace netty::udt
