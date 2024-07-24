////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/client_poller.hpp"
#include "pfs/netty/posix/poll_poller.hpp"
#include "pfs/netty/posix/select_poller.hpp"

namespace netty {

template <>
client_poller<posix::poll_poller>::client_poller ()
{
    init_callbacks();
}

template <>
client_poller<posix::select_poller>::client_poller ()
{
    init_callbacks();
}

} // namespace netty
