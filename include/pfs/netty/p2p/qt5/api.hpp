////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.11.19 Initial version.
//      2022.08.15 Changed signatures for startup/cleanup.
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

    static bool startup (std::error_code & /*ec*/) { return true; }
    static void cleanup (std::error_code & /*ec*/) {}
};

}}} // namespace netty::p2p::qt5
