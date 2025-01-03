////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/conn_status.hpp"
#include "pfs/netty/property.hpp"
#include "pfs/netty/posix/inet_socket.hpp"

namespace netty {
namespace posix {

class tcp_listener;

/**
 * POSIX Inet TCP socket
 */
class tcp_socket: public inet_socket
{
    friend class tcp_listener;

protected:
    /**
     * Constructs POSIX TCP accepted socket.
     */
    tcp_socket (socket_id sock, socket4_addr const & saddr);

public:
    tcp_socket (tcp_socket const & s) = delete;
    tcp_socket & operator = (tcp_socket const & s) = delete;

    /**
     * Constructs uninitialized (invalid) TCP socket.
     */
    NETTY__EXPORT tcp_socket (uninitialized);

    tcp_socket (property_map_t const & /*props*/, error * perr = nullptr)
        : tcp_socket()
    {}

    NETTY__EXPORT tcp_socket ();
    NETTY__EXPORT tcp_socket (tcp_socket && s);
    NETTY__EXPORT tcp_socket & operator = (tcp_socket && s);
    NETTY__EXPORT ~tcp_socket ();

    /**
     * Connects to the TCP server.
     *
     * @return @c conn_status::failure if error occurred while connecting,
     *         @c conn_status::success if connection established successfully or
     *         @c conn_status::in_progress if connection in progress.
     */
    NETTY__EXPORT conn_status connect (socket4_addr const & saddr, error * perr = nullptr);

    /**
     * Shutdown connection.
     */
    NETTY__EXPORT void disconnect (error * perr = nullptr);
};

}} // namespace netty::posix
