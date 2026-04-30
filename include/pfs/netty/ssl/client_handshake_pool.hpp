////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.29 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "basic_handshake_pool.hpp"

NETTY__NAMESPACE_BEGIN

namespace ssl {

class client_handshake_pool: public basic_handshake_pool
{
public:
    callback_t<void (socket_id, error const &)> on_failure;
    callback_t<void (socket_type &&)> on_connected;
    callback_t<void (socket4_addr, connection_failure_reason)> on_connection_refused;

public:
    NETTY__EXPORT client_handshake_pool ();
};

} // namespace ssl

NETTY__NAMESPACE_END
