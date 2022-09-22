////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.28 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/error.hpp"
#include "pfs/netty/p2p/udt/poller.hpp"
#include "newlib/core.hpp"
#include "newlib/udt.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"

namespace netty {
namespace p2p {
namespace udt {

poller::poller ()
{
    _eid = UDT::epoll_create();

    if (_eid < 0) {
        throw error{
              make_error_code(errc::poller_error)
            , error_string(tr::_("creation failure"))
        };
    }
}

poller::poller (poller &&) = default;
poller & poller::operator = (poller &&) = default;

poller::~poller ()
{
    if (_eid != CUDT::ERROR) {
        UDT::epoll_release(_eid);
        _eid = CUDT::ERROR;
    }
}

std::string poller::error_string (std::string const & reason)
{
    return fmt::format("poller failure: {}: {} ({})"
        , reason
        , UDT::getlasterror().getErrorMessage()
        , UDT::getlasterror().getErrorCode());
}

void poller::add (socket_type const & u, int events)
{
    LOG_TRACE_3("POLLER ADD: {}", u.native());

    auto rc = UDT::epoll_add_usock(_eid, u.native(), & events);

    if (rc < 0) {
        throw error {
              make_error_code(errc::poller_error)
            , error_string(tr::_("add socket failure"))
        };
    }
}

void poller::remove (socket_type const & u)
{
    LOG_TRACE_3("POLLER REMOVE: {}", u.native());

    auto rc = UDT::epoll_remove_usock(_eid, u.native());

    if (rc < 0) {
        throw error {
              make_error_code(errc::poller_error)
            , error_string(tr::_("remove socket failure"))
        };
    }
}

int poller::wait (std::chrono::milliseconds millis)
{
    PFS__ASSERT(_eid != CUDT::ERROR, "");

    _readfds.clear();
    _writefds.clear();

    auto rc = UDT::epoll_wait(_eid
        , & _readfds
        , & _writefds
        , millis.count()
        , nullptr, nullptr);

    if (rc < 0) {
        if (UDT::getlasterror().getErrorCode() != UDT::ERRORINFO::ETIMEOUT) {
            throw error {
                  make_error_code(errc::poller_error)
                , error_string(tr::_("wait failure"))};
        } else {
            rc = 0;
        }
    }

    return rc;
}

// Sockets with exceptions are returned to both read and write sets.
void poller::process_events (input_callback_type && input_callback
    , output_callback_type && output_callback)
{
    if (input_callback && !_readfds.empty()) {
        for (UDTSOCKET u: _readfds) {
            input_callback(u);
        }
    }

    if (output_callback && !_writefds.empty()) {
        for (UDTSOCKET u: _writefds) {
            output_callback(u);
        }
    }
}

}}} // namespace netty::p2p::udt
