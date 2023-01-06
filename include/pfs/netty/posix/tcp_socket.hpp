////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/posix/inet_socket.hpp"

namespace netty {
namespace posix {

class tcp_server;

enum class conn_status: std::int8_t {
      failure     = -1
    , success     =  0
    , in_progress =  1
};

/**
 * POSIX Inet TCP socket
 */
class tcp_socket: public inet_socket
{
    friend class tcp_server;

protected:
    /**
     * Constructs POSIX TCP accepted socket.
     */
    tcp_socket (native_type sock, socket4_addr const & saddr);

    /**
     * Constructs unitialized (invalid) TCP socket.
     */
    tcp_socket (bool);

public:
    tcp_socket (tcp_socket const & s) = delete;
    tcp_socket & operator = (tcp_socket const & s) = delete;

    NETTY__EXPORT tcp_socket ();

    NETTY__EXPORT tcp_socket (tcp_socket && s);
    NETTY__EXPORT tcp_socket & operator = (tcp_socket && s);
    NETTY__EXPORT ~tcp_socket ();

    /**
     * Connects to the TCP server.
     *
     * @return @c -1 if error occurred while connecting, @c 0 if connection
     *         established successfully or @c 1 if connection in progress.
     */
    NETTY__EXPORT conn_status connect (socket4_addr const & saddr, error * perr = nullptr);

    /**
     * Shutdown connection.
     */
    NETTY__EXPORT void disconnect (error * perr = nullptr);
};

}} // namespace netty::posix
