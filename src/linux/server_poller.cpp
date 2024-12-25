////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/server_poller.hpp"
#include "pfs/netty/linux/epoll_poller.hpp"

namespace netty {

template <>
server_poller<linux_os::epoll_poller>::server_poller (std::function<socket_id(socket_id, bool &)> && accept_proc)
{
    init_callbacks(std::move(accept_proc));
}

} // namespace netty
