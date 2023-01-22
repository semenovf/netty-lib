////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/exports.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include "pfs/netty/udt/udt_socket.hpp"

namespace netty {
namespace udt {

class basic_udt_server: public basic_socket
{
protected:
    /**
     * Constructs invalid (uninitialized) TCP server.
     */
    NETTY__EXPORT basic_udt_server ();

    /**
     * Constructs UDT server and bind to the specified address.
     *
     * @param saddr Bind address.
     * @param mtu Maximum transfer unit.
     * @param exp_max_counter Max socket expiration counter. Affects the
     *        interval when accepted socket becomes BROKEN.
     * @param exp_threshold Socket (peer, accepted) expiration threshold.
     *        Affects the interval when accepted socket becomes BROKEN
     *        (conjunction with @c exp_max_counter).
     */
    NETTY__EXPORT basic_udt_server (socket4_addr const & saddr, int mtu
        , int exp_max_counter, std::chrono::milliseconds exp_threshold);

    /**
     * Constructs UDT server, bind to the specified address and start
     * listening
     */
    NETTY__EXPORT basic_udt_server (socket4_addr const & addr, int backlog
        , int mtu, int exp_max_counter, std::chrono::milliseconds exp_threshold);

protected:
    /**
     * Aaccept a connection on a server socket.
     */
    NETTY__EXPORT void accept (basic_udt_socket & result, error * perr = nullptr);

protected:
    NETTY__EXPORT static void accept (native_type listener_sock
        , basic_udt_socket & result, error * perr = nullptr);

public:
    NETTY__EXPORT ~basic_udt_server ();

    /**
     * Listen for connections on a socket.
     *
     * @param backlog The maximum length to which the queue of pending
     *        connections may grow.
     */
    NETTY__EXPORT bool listen (int backlog, error * perr = nullptr);
};

template <int MTU = 1500>
class udt_server: public basic_udt_server
{
public:
    using udt_socket_type = udt_socket<MTU>;

public:
    /**
     * Constructs invalid (uninitialized) UDT server.
     */
    udt_server () : basic_udt_server() {}

    udt_server (socket4_addr const & saddr, int exp_max_counter
        , std::chrono::milliseconds exp_threshold)
        : basic_udt_server(saddr, MTU, exp_max_counter, exp_threshold)
    {}

    udt_server (socket4_addr const & saddr, int backlog, int exp_max_counter
        , std::chrono::milliseconds exp_threshold)
        : basic_udt_server(saddr, backlog, MTU, exp_max_counter, exp_threshold)
    {}

    udt_server (socket4_addr const & saddr)
        : basic_udt_server(saddr, MTU, default_exp_counter(), default_exp_threshold())
    {}

    udt_server (socket4_addr const & saddr, int backlog)
        : basic_udt_server(saddr, backlog, MTU, default_exp_counter(), default_exp_threshold())
    {}

    ~udt_server () = default;

    udt_socket_type accept (error * perr = nullptr)
    {
        udt_socket_type result(uninitialized{});
        basic_udt_server::accept(result, perr);
        return result;
    }

    void accept (basic_udt_socket & result, error * perr = nullptr);

public:
    static udt_socket_type accept (native_type listener_sock
        , error * perr = nullptr)
    {
        udt_socket_type result(uninitialized{});
        basic_udt_server::accept(listener_sock, result, perr);
        return result;
    }
};

}} // namespace netty::udt
