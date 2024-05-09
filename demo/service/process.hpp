////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "service.hpp"
#include <pfs/log.hpp>

bool process (server_connection_context & conn, echo const & e)
{
    message_serializer_t ms {echo {e.text}};
    service_t::client::output_envelope_type env {message_enum::echo, ms.take()};
    conn.server->enqueue(conn.sock, env.take());
    return true;
}

bool process (client_connection_context &, echo const & e)
{
    LOGD("echo", "{}", e.text);
    return true;
}
