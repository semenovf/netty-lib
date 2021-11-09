////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019 - 2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.11.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "poller.hpp"
#include "udp_socket.hpp"

namespace pfs {
namespace net {
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

    static bool startup ();
    static void cleanup ();
};

}}}} // namespace pfs::net::p2p::udt
