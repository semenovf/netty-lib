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

struct ProcessTraits
{
    template <typename Traits>
    static bool process (typename Traits::server_connection_context & conn, echo const & e)
    {
        message_serializer_t ms {echo {e.text}};
        output_envelope_t env {message_enum::echo, ms.take()};
        conn.server->enqueue(conn.sock, env.take());
        return true;
    }

    template <typename Traits>
    static bool process (typename Traits::client_connection_context &, echo const & e)
    {
        LOGD("echo", "{}", e.text);
        return true;
    }
};

template <typename ConnectionContext>
bool process (ConnectionContext & conn, echo const & e)
{
    return ProcessTraits::process<typename ConnectionContext::traits_type>(conn, e);
}
