////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
//      2024.07.29 `basic_udt_server` renamed to `basic_udt_listener`,
//                 `udt_listener` renamed to `udt_listener`.
//      2025.01.10 Removed deprecated constructors.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/exports.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/property.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include "pfs/netty/udt/udt_socket.hpp"

namespace netty {
namespace udt {

class udt_listener: public udt_socket
{
public:
    using listener_id = udt_socket::listener_id;
    using socket_id = udt_socket::socket_id;
    using peer_socket_type = udt_socket;

public:
    /**
     * Constructs invalid (uninitialized) UDT server.
     */
    NETTY__EXPORT udt_listener ();

    /**
     * Constructs UDT listener and bind to the specified address.
     *
     * @param saddr Bind address.
     * @param mtu Maximum transfer unit.
     * @param exp_max_counter Max socket expiration counter. Affects the
     *        interval when accepted socket becomes BROKEN.
     * @param exp_threshold Socket (peer, accepted) expiration threshold.
     *        Affects the interval when accepted socket becomes BROKEN
     *        (conjunction with @c exp_max_counter).
     */
    NETTY__EXPORT udt_listener (socket4_addr const & saddr, int mtu
        , int exp_max_counter, std::chrono::milliseconds exp_threshold
        , error * perr = nullptr);

    NETTY__EXPORT ~udt_listener ();

public:
    NETTY__EXPORT listener_id id () const noexcept;

    /**
     * Listen for connections on a socket.
     *
     * @param backlog The maximum length to which the queue of pending
     *        connections may grow.
     */
    NETTY__EXPORT bool listen (int backlog, error * perr = nullptr);

public: // static
    NETTY__EXPORT static peer_socket_type accept (
          listener_id listener_sock // really here not a listener socket, already accepted socket
        , error * perr = nullptr);

    NETTY__EXPORT static peer_socket_type accept_nonblocking (
          listener_id listener_sock // really here not a listener socket, already accepted socket
        , error * perr = nullptr);
};

}} // namespace netty::udt
