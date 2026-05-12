////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.05.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "socket4_addr.hpp"
#include <pfs/optional.hpp>
#include <string>

#if NETTY__OPENSSL3_ENABLED
#   include "ssl/tls_options.hpp"
#endif

NETTY__NAMESPACE_BEGIN

struct listener_options
{
    socket4_addr saddr = socket4_addr {inet4_addr::any_addr_value, 0};
    int backlog = 10;

#if NETTY__OPENSSL3_ENABLED
    ssl::tls_options tls;
#endif
};

NETTY__NAMESPACE_END
