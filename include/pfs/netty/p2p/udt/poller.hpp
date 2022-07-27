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
#include "udp_socket.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace netty {
namespace p2p {
namespace udt {

class poller
{
    using UDTSOCKET = decltype(udp_socket{}.id());

public:
    enum event_enum {
          POLL_IN_EVENT  = 0x1
        , POLL_OUT_EVENT = 0x4
        , POLL_ERR_EVENT = 0x8
    };

    using input_callback_type  = std::function<void(UDTSOCKET)>;
    using output_callback_type = std::function<void(UDTSOCKET)>;

private:
    class impl;

    std::unique_ptr<impl> _p;
    std::string _name;

public:
    mutable std::function<void (std::string const &)> failure;

public:
    NETTY__EXPORT poller (std::string const & name);
    NETTY__EXPORT ~poller ();

    poller (poller const &) = delete;
    poller & operator = (poller const &) = delete;

    NETTY__EXPORT poller (poller &&);
    NETTY__EXPORT poller & operator = (poller &&);

    NETTY__EXPORT bool initialize ();
    NETTY__EXPORT void release ();
    NETTY__EXPORT void add (UDTSOCKET u, int events = POLL_IN_EVENT | POLL_OUT_EVENT | POLL_ERR_EVENT);
    NETTY__EXPORT void remove (UDTSOCKET u);
    NETTY__EXPORT int wait (std::chrono::milliseconds millis);

    NETTY__EXPORT void process_events (input_callback_type && in, output_callback_type && out);

private:
    NETTY__EXPORT void failure_helper (std::string const & reason);
};

}}} // namespace netty::p2p::udt
