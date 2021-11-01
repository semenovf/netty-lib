////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.28 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "udp_socket.hpp"
#include "pfs/emitter.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace pfs {
namespace net {
namespace p2p {
namespace udt {

class poller
{
    using UDTSOCKET = decltype(udp_socket{}.id());

public:
    using input_callback_type  = std::function<void(UDTSOCKET)>;
    using output_callback_type = std::function<void(UDTSOCKET)>;

private:
    class impl;

    std::unique_ptr<impl> _p;

public:
    pfs::emitter_mt<std::string const &> failure;

public:
    poller ();
    ~poller ();

    poller (poller const &) = delete;
    poller & operator = (poller const &) = delete;

    poller (poller &&);
    poller & operator = (poller &&);

    bool initialize ();
    void add (UDTSOCKET u);
    void remove (UDTSOCKET u);
    int wait (std::chrono::milliseconds millis);

    void process_events (input_callback_type && in, output_callback_type && out);
};

}}}} // namespace pfs::net::p2p::udt
