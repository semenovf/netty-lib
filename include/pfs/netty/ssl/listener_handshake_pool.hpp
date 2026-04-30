////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "basic_handshake_pool.hpp"

NETTY__NAMESPACE_BEGIN

namespace ssl {

class listener_handshake_pool: public basic_handshake_pool
{
public:
    NETTY__EXPORT listener_handshake_pool ();

public:
    callback_t<void (socket_id, error const &)> on_failure;
    callback_t<void (socket_type &&)> on_accepted;
};

} // namespace ssl

NETTY__NAMESPACE_END
