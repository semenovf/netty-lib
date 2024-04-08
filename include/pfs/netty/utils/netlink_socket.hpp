////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.02.16 Initial version.
//      2024.04.08 Moved to `utils` namespace.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "error.hpp"
#include "exports.hpp"

#if NETTY__LIBMNL_ENABLED
struct mnl_socket;
#endif

namespace netty {
namespace utils {

/**
 * Netlink socket
 */
class netlink_socket
{
public:
    using native_type = int;
    static native_type constexpr kINVALID_SOCKET = -1;

    enum class type_enum {
          unknown = -1
        , route = 0 // NETLINK_ROUTE
    };

private:
#if NETTY__LIBMNL_ENABLED
    mnl_socket * _socket { nullptr };
#else
    native_type _socket { kINVALID_SOCKET };
#endif

public:
    /**
     * Constructs invalid Netlink socket.
     */
    netlink_socket ();

    /**
     * Constructs Netlink socket.
     */
    netlink_socket (type_enum netlinktype);

    netlink_socket (netlink_socket const &) = delete;
    netlink_socket & operator = (netlink_socket const &) = delete;

    NETTY__EXPORT ~netlink_socket ();

    NETTY__EXPORT netlink_socket (netlink_socket &&);
    NETTY__EXPORT netlink_socket & operator = (netlink_socket &&);

public:
    /**
     *  Checks if Netlink socket is valid
     */
    NETTY__EXPORT operator bool () const noexcept;

    NETTY__EXPORT native_type native () const noexcept;

    /**
     * Receive data from Netlink socket.
     */
    NETTY__EXPORT int recv (char * data, int len, error * perr = nullptr);

    /**
     * Send @a req request with @a len length on a Netlink socket.
     */
    NETTY__EXPORT int send (char const * req, int len, error * perr = nullptr);
};

}} // namespace netty::utils
