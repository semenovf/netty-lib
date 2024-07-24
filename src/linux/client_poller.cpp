////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/client_poller.hpp"
#include "pfs/netty/linux/epoll_poller.hpp"

namespace netty {

template <>
client_poller<linux_os::epoll_poller>::client_poller ()
{
    init_callbacks();
}

} // namespace netty
