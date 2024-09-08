////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.07.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "netty/server_poller.hpp"
#include "pfs/netty/posix/poll_poller.hpp"
#include "pfs/netty/posix/select_poller.hpp"

namespace netty {

#if NETTY__POLL_ENABLED
template <>
server_poller<posix::poll_poller>::server_poller (std::function<native_socket_type(native_socket_type, bool &)> && accept_proc)
{
    init_callbacks(std::move(accept_proc));
}
#endif

#if NETTY__SELECT_ENABLED
template <>
server_poller<posix::select_poller>::server_poller (std::function<native_socket_type(native_socket_type, bool &)> && accept_proc)
{
    init_callbacks(std::move(accept_proc));
}
#endif

} // namespace netty

