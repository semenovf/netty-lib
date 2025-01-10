////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/client_poller.hpp"
#include "pfs/netty/enet/enet_poller.hpp"

namespace netty {

template <>
client_poller<enet::enet_poller>::client_poller ()
    : client_poller(new enet::enet_poller)
{}

} // namespace netty
