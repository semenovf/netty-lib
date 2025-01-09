////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
// FIXME DEPRECATED
#include "netty/client_poller.hpp"
#include "pfs/netty/posix/select_poller.hpp"

#if NETTY__POLL_ENABLED
#   include "pfs/netty/posix/poll_poller.hpp"
#endif

namespace netty {

#if NETTY__POLL_ENABLED
template <>
client_poller<posix::poll_poller>::client_poller ()
{
    init_callbacks();
}
#endif

#if NETTY__SELECT_ENABLED
template <>
client_poller<posix::select_poller>::client_poller ()
{
    init_callbacks();
}
#endif

} // namespace netty
