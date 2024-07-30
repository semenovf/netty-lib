////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.18 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/error.hpp>
#include <pfs/netty/exports.hpp>
#include <pfs/netty/inet4_addr.hpp>
#include <pfs/netty/send_result.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <pfs/netty/uninitialized.hpp>
#include <QUdpSocket>
#include <memory>

namespace netty {
namespace qt5 {

/**
 * Qt5 Inet UDP socket
 */
class udp_socket
{
public:
    using native_type = qintptr; // See QAbstractSocket::socketDescriptor()

protected:
    std::unique_ptr<QUdpSocket> _socket;

protected:
    /**
      * Joins the multicast group specified by @a group on the default interface
      * chosen by the operating system.
      *
      * @param group_addr Multicast group address.
      * @param local_addr Local address to bind and to set the multicast interface.
      *
      * @return @c true if successful; otherwise it returns @c false.
      */
    bool join (socket4_addr const & group_saddr
        , inet4_addr const & local_addr, error * perr = nullptr);

    /**
     * Leaves the multicast group specified by @a group groupAddress on the
     * default interface chosen by the operating system.
     *
     * @return @c true if successful; otherwise it returns @c false.
     */
    bool leave (socket4_addr const & group_saddr
        , inet4_addr const & local_addr, error * perr = nullptr);

    bool enable_broadcast (bool enable, error * perr = nullptr);

public:
    udp_socket (udp_socket const &) = delete;
    udp_socket & operator = (udp_socket const &) = delete;

    /**
     * Constructs uninitialized (invalid) UDP socket.
     */
    NETTY__EXPORT udp_socket (uninitialized);

    NETTY__EXPORT udp_socket ();
    NETTY__EXPORT udp_socket (udp_socket &&);
    NETTY__EXPORT udp_socket & operator = (udp_socket &&);
    NETTY__EXPORT ~udp_socket ();

    /**
     *  Checks if socket is valid
     */
    NETTY__EXPORT operator bool () const noexcept;

    NETTY__EXPORT native_type native () const noexcept;

    NETTY__EXPORT int available (error * perr = nullptr) const;

    NETTY__EXPORT int recv_from (char * data, int len
        , socket4_addr * saddr = nullptr, error * perr = nullptr);

    NETTY__EXPORT send_result send_to (socket4_addr const & dest
        , char const * data, int len, error * perr = nullptr);
};

}} // namespace netty::qt5

