////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.28 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
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
    poller (std::string const & name);
    ~poller ();

    poller (poller const &) = delete;
    poller & operator = (poller const &) = delete;

    poller (poller &&);
    poller & operator = (poller &&);

    bool initialize ();
    void release ();
    void add (UDTSOCKET u, int events = POLL_IN_EVENT | POLL_OUT_EVENT | POLL_ERR_EVENT);
    void remove (UDTSOCKET u);
    int wait (std::chrono::milliseconds millis);

    void process_events (input_callback_type && in, output_callback_type && out);

private:
    void failure_helper (std::string const & reason);
};

}}} // namespace netty::p2p::udt
