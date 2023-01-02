////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.28 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/exports.hpp"
#include "tcp_socket.hpp"
#include <chrono>
#include <functional>
#include <set>
#include <string>

namespace netty {
namespace p2p {
namespace posix {

class poller
{
    using socket_type = tcp_socket;
    using socket_id = socket_type::native_type;

public:
    enum event_enum {
          POLL_IN_EVENT  = 0x1
        , POLL_OUT_EVENT = 0x4
        , POLL_ERR_EVENT = 0x8
    };

    using input_callback_type  = std::function<void(socket_id)>;
    using output_callback_type = std::function<void(socket_id)>;

private:
    int _eid {-1};

    std::set<socket_id> _readfds;
    std::set<socket_id> _writefds;

    // For system sockets on Linux, developers may choose to watch individual
    // events from EPOLLIN (read), EPOLLOUT (write), and EPOLLERR (exceptions).
    // When using epoll_remove_ssock, if the socket is waiting on multiple
    // events, only those specified in events are removed.
    // The events can be a combination (with "|" operation).
    // For all other situations, the parameter events is ignored and all events
    // will be watched.
    // For compatibility, set them all.
    // int events {UDT_EPOLL_IN | UDT_EPOLL_OUT | UDT_EPOLL_ERR};

private:
    poller (poller const &) = delete;
    poller & operator = (poller const &) = delete;

public:
    NETTY__EXPORT poller ();
    NETTY__EXPORT ~poller ();

    NETTY__EXPORT poller (poller &&);
    NETTY__EXPORT poller & operator = (poller &&);

    NETTY__EXPORT void add (socket_type const & u, int events = POLL_IN_EVENT | POLL_OUT_EVENT | POLL_ERR_EVENT);
    NETTY__EXPORT void remove (socket_type const & u);
    NETTY__EXPORT int wait (std::chrono::milliseconds millis);

    NETTY__EXPORT void process_events (input_callback_type && in, output_callback_type && out);

private:
    NETTY__EXPORT std::string error_string (std::string const & reason);
};

}}} // namespace netty::p2p::posix
