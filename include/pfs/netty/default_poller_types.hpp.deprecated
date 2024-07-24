////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "poller_types.hpp"

namespace netty {

#if __ANDROID__
#   if NETTY__POLL_ENABLED
        using client_poller_type = client_poll_poller_type;
        using server_poller_type = server_poll_poller_type;
#   elif NETTY__SELECT_ENABLED
        using client_poller_type = client_select_poller_type;
        using server_poller_type = server_select_poller_type;
#   else
#       error "No poller available for Android"
#   endif
#elif _MSC_VER
#   if NETTY__SELECT_ENABLED
        using client_poller_type = client_select_poller_type;
        using server_poller_type = server_select_poller_type;
#   else
#       error "No poller available for MSVC"
#   endif
#elif __linux__
#   if NETTY__EPOLL_ENABLED
        using client_poller_type = client_epoll_poller_type;
        using server_poller_type = server_epoll_poller_type;
#   elif NETTY__POLL_ENABLED
        using client_poller_type = client_poll_poller_type;
        using server_poller_type = server_poll_poller_type;
#   elif NETTY__SELECT_ENABLED
        using client_poller_type = client_select_poller_type;
        using server_poller_type = server_select_poller_type;
#   else
#       error "No poller available for Linux"
#   endif
#else
#   error "Unsupported platform"
#endif

} // namespace netty

