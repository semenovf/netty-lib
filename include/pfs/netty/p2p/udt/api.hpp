////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.11.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/exports.hpp"
#include "poller.hpp"
#include "udp_socket.hpp"

namespace netty {
namespace p2p {
namespace udt {

class api final
{
public:
    using poller_type = poller;
    using socket_type = udp_socket;

public:
    api (api const &) = delete;
    api (api &&) = delete;
    api & operator = (api const &) = delete;
    api & operator = (api &&) = delete;

    static NETTY__EXPORT bool startup ();
    static NETTY__EXPORT void cleanup ();
};

}}} // namespace netty::p2p::udt
