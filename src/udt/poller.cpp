////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.28 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/fmt.hpp"
#include "pfs/net/p2p/udt/poller.hpp"
#include "lib/core.h"
#include "lib/udt.h"
#include <set>
#include <cassert>

#define PFS_NET__P2P_TRACE_LEVEL 3
#include "pfs/net/p2p/trace.hpp"

namespace pfs {
namespace net {
namespace p2p {
namespace udt {

class poller::impl
{
public:
    int eid {CUDT::ERROR};

    std::set<UDTSOCKET> readfds;
    std::set<UDTSOCKET> writefds;

    // For system sockets on Linux, developers may choose to watch individual
    // events from EPOLLIN (read), EPOLLOUT (write), and EPOLLERR (exceptions).
    // When using epoll_remove_ssock, if the socket is waiting on multiple
    // events, only those specified in events are removed.
    // The events can be a combination (with "|" operation).
    // For all other situations, the parameter events is ignored and all events
    // will be watched.
    // For compatibility, set them all.
    int events {UDT_EPOLL_IN | UDT_EPOLL_OUT | UDT_EPOLL_ERR};
};

poller::poller (std::string const & name)
    : _p(new poller::impl{})
    , _name(name)
{}

poller::poller (poller &&) = default;
poller & poller::operator = (poller &&) = default;

poller::~poller ()
{
    if (_p->eid == CUDT::ERROR) {
        UDT::epoll_release(_p->eid);
        _p->eid = CUDT::ERROR;
    }
}

void poller::failure_helper (std::string const & reason)
{
    failure(fmt::format("poller #{}: {}: {} ({})"
        , _name
        , reason
        , UDT::getlasterror().getErrorMessage()
        , UDT::getlasterror().getErrorCode()));
}

bool poller::initialize ()
{
    bool success = true;
    _p->eid = UDT::epoll_create();

    if (_p->eid < 0) {
        success = false;
        failure_helper("creation failure");
    }

    return success;
}

void poller::add (UDTSOCKET u)
{
    TRACE_3("POLLER #{}: ADD: {}", _name, u);

    auto rc = UDT::epoll_add_usock(_p->eid, u, & _p->events);

    if (rc < 0) {
        failure_helper("add socket failure");
    }
}

void poller::remove (UDTSOCKET u)
{
    TRACE_3("POLLER #{}: REMOVE: {}", _name, u);

    auto rc = UDT::epoll_remove_usock(_p->eid, u);

    if (rc < 0) {
        failure_helper("remove socket failure");
    }
}

int poller::wait (std::chrono::milliseconds millis)
{
    assert(_p->eid != CUDT::ERROR);

    _p->readfds.clear();
    _p->writefds.clear();

    auto rc = UDT::epoll_wait(_p->eid
        , & _p->readfds
        , & _p->writefds
        , millis.count()
        , nullptr, nullptr);

    if (rc > 0) {
        TRACE_3("POLLER #{}: WAITed: count = {}", _name, rc);
    }

    if (rc < 0) {
        if (UDT::getlasterror().getErrorCode() != UDT::ERRORINFO::ETIMEOUT) {
            failure_helper("wait failure");
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
    if (input_callback && !_p->readfds.empty()) {
        for (UDTSOCKET u: _p->readfds) {
            input_callback(u);
        }
    }

    if (output_callback && !_p->writefds.empty()) {
        for (UDTSOCKET u: _p->writefds) {
            output_callback(u);
        }
    }
}

}}}} // namespace pfs::net::p2p::udt
