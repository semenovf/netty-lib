////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.29 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/client_poller.hpp"
#include "pfs/netty/udt/epoll_poller.hpp"

namespace netty {

template <>
client_poller<udt::epoll_poller>::client_poller ()
{
    init_callbacks();
}

} // namespace netty
