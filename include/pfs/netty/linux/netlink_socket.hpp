////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.02.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/error.hpp"
#include "pfs/netty/exports.hpp"

struct mnl_socket;

namespace netty {
namespace linux_os {

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
    mnl_socket * _socket { nullptr };

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

}} // namespace netty::linux_os
