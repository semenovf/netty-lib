////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "tls_options.hpp"
#include "../error.hpp"
#include "../exports.hpp"
#include "../conn_status.hpp"
#include "../namespace.hpp"
// #include "../send_result.hpp"
#include "../socket4_addr.hpp"
#include <memory>

#if _MSC_VER
#   include <winsock2.h>
#endif

NETTY__NAMESPACE_BEGIN

namespace ssl {

class tls_listener;
class client_handshake_pool;
class listener_handshake_pool;

class tls_socket
{
    friend class tls_listener;
    friend class client_handshake_pool;
    friend class listener_handshake_pool;

    class impl;

public:
#if _MSC_VER
    using socket_id = SOCKET;
    static socket_id const kINVALID_SOCKET = INVALID_SOCKET;
#else
    using socket_id = int;
    static socket_id constexpr kINVALID_SOCKET = -1;
#endif

private:
    std::unique_ptr<impl> _d;

public:
    /**
     * Constructs invalid TLS socket
     */
    NETTY__EXPORT tls_socket ();

    /**
     * For internal use only.
     */
    NETTY__EXPORT tls_socket (std::unique_ptr<impl> d);

    tls_socket (tls_socket const &) = delete;
    tls_socket & operator = (tls_socket const &) = delete;
    NETTY__EXPORT tls_socket (tls_socket &&) noexcept;
    NETTY__EXPORT tls_socket & operator = (tls_socket &&) noexcept;

    NETTY__EXPORT ~tls_socket ();

public:
    /**
     *  Checks if socket is valid
     */
    NETTY__EXPORT operator bool () const noexcept;

    NETTY__EXPORT socket_id id () const noexcept;

    NETTY__EXPORT socket4_addr saddr () const noexcept;

    /**
     * Connects to the TCP server @a remote_addr.
     *
     * @return @c conn_status::failure if error occurred while connecting,
     *         @c conn_status::success if connection established successfully or
     *         @c conn_status::in_progress if connection in progress.
     */
    NETTY__EXPORT conn_status connect (socket4_addr const & remote_saddr, tls_options opts
        , error * perr = nullptr);

    NETTY__EXPORT int recv (char * data, int len, error * perr = nullptr);

//     /**
//      * Send @a data message with @a size on a socket.
//      *
//      * @param data Data to send.
//      * @param size Data size to send.
//      * @param overflow Flag that the send buffer is overflow (@c true).
//      * @param perr Pointer to structure to store error if occurred.
//      */
//     NETTY__EXPORT send_result send (char const * data, int len, error * perr = nullptr);
//
//     NETTY__EXPORT int recv_from (char * data, int len, socket4_addr * saddr = nullptr
//         , error * perr = nullptr);
//
//     /**
//      * See send description.
//      */
//     NETTY__EXPORT send_result send_to (socket4_addr const & dest, char const * data, int len
//         , error * perr = nullptr);
};

} // namespace ssl

NETTY__NAMESPACE_END

