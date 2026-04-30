////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.30 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "tls_socket.hpp"
#include "../error.hpp"
#include "../poller_types.hpp"
#include <unordered_map>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace ssl {

using reader_poller_t =
#if NETTY__EPOLL_ENABLED
        reader_epoll_poller_t;
#elif NETTY__POLL_ENABLED
        reader_poll_poller_t;
#elif NETTY__SELECT_ENABLED
        reader_select_poller_t;
#else
#   error "No reader poller implemented"
#endif

class basic_handshake_pool: protected reader_poller_t
{
protected:
    using socket_type = tls_socket;
    using socket_id = socket_type::socket_id;

    struct account
    {
        socket_type sock;
    };

private:
    std::unordered_map<socket_id, account> _accounts;
    std::vector<socket_id> _removable;

protected:
    NETTY__EXPORT basic_handshake_pool ();

    basic_handshake_pool (basic_handshake_pool &&) noexcept = delete;
    basic_handshake_pool & operator = (basic_handshake_pool &&) noexcept = delete;
    basic_handshake_pool (basic_handshake_pool const &) = delete;
    basic_handshake_pool & operator = (basic_handshake_pool const &) = delete;

public:
    NETTY__EXPORT void add (socket_type &&);
    NETTY__EXPORT void remove_later (socket_id);
    NETTY__EXPORT void apply_remove ();

    /**
     * @return Number of events occurred.
     */
    NETTY__EXPORT unsigned int step (error * perr = nullptr);

protected:
    account * locate_account (socket_id id);
};

} // namespace ssl

NETTY__NAMESPACE_END
