////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/error.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/inet4_addr.hpp"
#include "pfs/netty/send_result.hpp"
#include "pfs/netty/socket4_addr.hpp"

#if _MSC_VER
#   include <winsock2.h>
#endif

namespace netty {
namespace posix {

struct uninitialized {};

/**
 * POSIX inet socket
 */
class inet_socket
{
public:
#if _MSC_VER
    using native_type = SOCKET;
    static native_type const kINVALID_SOCKET = INVALID_SOCKET;
#else
    using native_type = int;
    static native_type constexpr kINVALID_SOCKET = -1;
#endif

protected:
    enum class type_enum {
          unknown
        , stream = 0x001
        , dgram  = 0x002
    };

protected:
    native_type _socket { kINVALID_SOCKET };

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
    inet_socket (type_enum socktype);

    /**
     * Constructs POSIX socket from native socket.
     */
    inet_socket (native_type sock, socket4_addr const & saddr);

    inet_socket (inet_socket const &) = delete;
    inet_socket & operator = (inet_socket const &) = delete;

    NETTY__EXPORT ~inet_socket ();

    inet_socket (inet_socket &&);
    inet_socket & operator = (inet_socket &&);

protected:
    static bool bind (native_type sock, socket4_addr const & saddr, error * perr = nullptr);

public:
    /**
     *  Checks if socket is valid
     */
    NETTY__EXPORT operator bool () const noexcept;

    NETTY__EXPORT native_type native () const noexcept;

    NETTY__EXPORT socket4_addr saddr () const noexcept;

    NETTY__EXPORT int available (error * perr = nullptr) const;

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
    NETTY__EXPORT send_result send_to (socket4_addr const & dest
        , char const * data, int len, error * perr = nullptr);
};

}} // namespace netty::posix
