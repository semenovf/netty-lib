////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
//      2024.07.29 `basic_udt_server` renamed to `basic_udt_listener`,
//                 `udt_listener` renamed to `udt_listener`
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
    using listener_id = udt_socket::native_type;
    using socket_id = udt_socket::native_type;
    using peer_socket_type = udt_socket;

private:
    void init (socket4_addr const & saddr, error * perr = nullptr);

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

    /**
     * Constructs UDT listener, bind to the specified address and start
     * listening
     */
    NETTY__EXPORT udt_listener (socket4_addr const & addr, int backlog
        , int mtu, int exp_max_counter, std::chrono::milliseconds exp_threshold
        , error * perr = nullptr);

    /**
     * Constructs UDT listener with specified properties. Accepts the following parameters:
     *      - "mtu" (std::intmax_t)  - MTU;
     *      - "exp_max_counter" (std::intmax_t) - max socket expiration counter;
     *      - "exp_threshold" (std::intmax_t) - socket (peer, accepted) expiration threshold in milliseconds.
     */
    NETTY__EXPORT udt_listener (socket4_addr const & addr
        , property_map_t const & props = property_map_t{}, error * perr = nullptr);

    /**
     * Constructs UDT listener with specified properties, bind to the specified address and start
     * listening.
     */
    NETTY__EXPORT udt_listener (socket4_addr const & addr, int backlog
        , property_map_t const & props = property_map_t{}, error * perr = nullptr);

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
