////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/error.hpp>
#include <pfs/netty/exports.hpp>
#include <pfs/netty/inet4_addr.hpp>
#include <pfs/netty/namespace.hpp>
#include <pfs/netty/send_result.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <pfs/netty/uninitialized.hpp>

#if _MSC_VER
#   include <winsock2.h>
#endif

NETTY__NAMESPACE_BEGIN

namespace posix {

/**
 * POSIX inet socket
 */
class inet_socket
{
public:
#if _MSC_VER
    using socket_id = SOCKET;
    static socket_id const kINVALID_SOCKET = INVALID_SOCKET;
#else
    using socket_id = int;
    static socket_id constexpr kINVALID_SOCKET = -1;
#endif

protected:
    enum class type_enum {
          unknown
        , stream = 0x001
        , dgram  = 0x002
    };

protected:
    socket_id _socket { kINVALID_SOCKET };

    // Bound address for server.
    // Server address for connected socket.
    socket4_addr _saddr;

protected:
    /**
     * Constructs invalid POSIX socket
     */
    inet_socket ();

    /**
     * Constructs POSIX socket.
     */
    inet_socket (type_enum socktype, error * perr = nullptr);

    /**
     * Constructs POSIX socket from native socket.
     */
    inet_socket (socket_id sock, socket4_addr const & saddr, error * perr = nullptr);

    inet_socket (inet_socket const &) = delete;
    inet_socket & operator = (inet_socket const &) = delete;

    NETTY__EXPORT ~inet_socket ();

    NETTY__EXPORT inet_socket (inet_socket &&) noexcept;
    NETTY__EXPORT inet_socket & operator = (inet_socket &&) noexcept;

protected:
    NETTY__EXPORT bool set_nonblocking (bool enable, error * perr);

protected:
    static bool check_socket_descriptor (socket_id sock, error * perr);
    static bool bind (socket_id sock, socket4_addr const & saddr, error * perr);
    static bool set_nonblocking (socket_id sock, bool enable, error * perr);
    static bool is_nonblocking (socket_id sock, error * perr);

public:
    /**
     *  Checks if socket is valid
     */
    NETTY__EXPORT operator bool () const noexcept;

    NETTY__EXPORT socket_id id () const noexcept;

    NETTY__EXPORT socket4_addr saddr () const noexcept;

    NETTY__EXPORT int recv (char * data, int len, error * perr = nullptr);

    /**
     * Send @a data message with @a size on a socket.
     *
     * @param data Data to send.
     * @param size Data size to send.
     * @param overflow Flag that the send buffer is overflow (@c true).
     * @param perr Pointer to structure to store error if occurred.
     */
    NETTY__EXPORT send_result send (char const * data, int len, error * perr = nullptr);

    NETTY__EXPORT int recv_from (char * data, int len, socket4_addr * saddr = nullptr
        , error * perr = nullptr);

    /**
     * See send description.
     */
    NETTY__EXPORT send_result send_to (socket4_addr const & dest, char const * data, int len
        , error * perr = nullptr);
};

} // namespace posix

NETTY__NAMESPACE_END
