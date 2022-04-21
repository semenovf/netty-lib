////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.11.19 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "udp_socket.hpp"

namespace netty {
namespace p2p {
namespace qt5 {

class api final
{
public:
    using socket_type = udp_socket;

public:
    api (api const &) = delete;
    api (api &&) = delete;
    api & operator = (api const &) = delete;
    api & operator = (api &&) = delete;

    static bool startup () { return true; }
    static void cleanup () {}
};

}}} // namespace netty::p2p::qt5
