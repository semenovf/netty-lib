////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.12.30 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/error.hpp"
#include "pfs/netty/p2p/posix/poller.hpp"
// #include "newlib/core.hpp"
// #include "newlib/udt.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"
#include <sys/epoll.h>

#if _MSC_VER
#   include <windows.h>
#else
#   include <sys/epoll.h>
#   include <unistd.h>
#endif

namespace netty {
namespace p2p {
namespace posix {

poller::poller ()
{
    // Since Linux 2.6.8, the size argument is ignored, but must be greater than zero;
    int size = 1024;
    _eid = ::epoll_create(size);

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
    if (_eid > 0) {
        ::close(_eid);
        _eid = -1;
    }
}

std::string poller::error_string (std::string const & reason)
{
    // FIXME
//     return fmt::format("poller failure: {}: {} ({})"
//         , reason
//         , UDT::getlasterror().getErrorMessage()
//         , UDT::getlasterror().getErrorCode());
    return std::string{"FIXME: POLLER ERROR"};
}

void poller::add (socket_type const & u, int events)
{
    LOG_TRACE_3("POLLER ADD: {}", u.native());

    // FIXME
//     auto rc = UDT::epoll_add_usock(_eid, u.native(), & events);
//
//     if (rc < 0) {
//         throw error {
//               make_error_code(errc::poller_error)
//             , error_string(tr::_("add socket failure"))
//         };
//     }
}

void poller::remove (socket_type const & u)
{
    LOG_TRACE_3("POLLER REMOVE: {}", u.native());

    auto rc = ::epoll_ctl(_eid, EPOLL_CTL_DEL, u.native(), nullptr);

    if (rc != 0) {
        throw error {
              pfs::get_last_system_error()
            , error_string(tr::_("remove socket from poller failure"))
        };
    }
}

int poller::wait (std::chrono::milliseconds millis)
{
    // FIXME
//     PFS__ASSERT(_eid != CUDT::ERROR, "");
//
//     _readfds.clear();
//     _writefds.clear();
//
//     auto rc = UDT::epoll_wait(_eid
//         , & _readfds
//         , & _writefds
//         , millis.count()
//         , nullptr, nullptr);
//
//     if (rc < 0) {
//         if (UDT::getlasterror().getErrorCode() != UDT::ERRORINFO::ETIMEOUT) {
//             throw error {
//                   make_error_code(errc::poller_error)
//                 , error_string(tr::_("wait failure"))};
//         } else {
//             rc = 0;
//         }
//     }
//
//     return rc;

    return 0;
}

// Sockets with exceptions are returned to both read and write sets.
void poller::process_events (input_callback_type && input_callback
    , output_callback_type && output_callback)
{
    // FIXME
//     if (input_callback && !_readfds.empty()) {
//         for (UDTSOCKET u: _readfds) {
//             input_callback(u);
//         }
//     }
//
//     if (output_callback && !_writefds.empty()) {
//         for (UDTSOCKET u: _writefds) {
//             output_callback(u);
//         }
//     }
}

}}} // namespace netty::p2p::posix
