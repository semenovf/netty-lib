////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/server_poller.hpp"
#include "pfs/netty/enet/enet_poller.hpp"

namespace netty {

template <>
server_poller<enet::enet_poller>::server_poller (std::function<native_socket_type(native_socket_type, bool &)> && accept_proc)
    : server_poller(std::make_shared<enet::enet_poller>(), std::move(accept_proc))
{}

} // namespace netty
