////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.26 Initial version.
//      2023.01.06 Renamed to `udt_socket` and refactored.
//      2024.07.29 Refactored `udt_socket`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/conn_status.hpp>
#include <pfs/netty/error.hpp>
#include <pfs/netty/exports.hpp>
#include <pfs/netty/send_result.hpp>
#include <pfs/netty/socket4_addr.hpp>
// #include <pfs/netty/uninitialized.hpp>
#include <chrono>
#include <string>
#include <vector>

namespace netty {
namespace udt {

class udt_listener;

class udt_socket
{
    friend class udt_listener;

public:
    // Typedef UDTSOCKET as defined in `udt.h`
    using UDTSOCKET = int;
    using listener_id = UDTSOCKET;
    using socket_id = UDTSOCKET;

    static UDTSOCKET const kINVALID_SOCKET = -1;

    using input_buffer_type = std::vector<char>;

private:
    socket_id _socket {kINVALID_SOCKET};

    // Bound address for listener.
    // Listener address for connected socket.
    socket4_addr _saddr;

    input_buffer_type _inpb;

protected:
    /**
     * Constructs UDT accepted socket.
     */
    udt_socket (socket_id sock, socket4_addr const & saddr);

    void init (int mtu, int exp_max_counter, std::chrono::milliseconds exp_threshold, error * perr = nullptr);

public:
    udt_socket (udt_socket const & other) = delete;
    udt_socket & operator = (udt_socket const & other) = delete;

    /**
     * Constructs uninitialized (invalid) UDT socket.
     */
    NETTY__EXPORT udt_socket ();

    NETTY__EXPORT udt_socket (udt_socket && other) noexcept;
    NETTY__EXPORT udt_socket & operator = (udt_socket && other) noexcept;

    /**
     * Constructs new UDT socket.
     *
     * @param type UDT socket type (only `type_enum::dgram` supported).
     * @param mtu MTU value.
     * @param exp_max_counter Max socket expiration counter. Affects the
     *        interval when accepted socket becomes BROKEN.
     * @param exp_threshold Socket (peer, accepted) expiration threshold.
     *        Affects the interval when accepted socket becomes BROKEN
     *        (conjunction with @c exp_max_counter).
     *
     * @note Descriptions for `exp_max_counter` and `exp_threshold` see in
     *       `udt/newlib/core.hpp`.
     */
    NETTY__EXPORT udt_socket (int mtu, int exp_max_counter, std::chrono::milliseconds exp_threshold
        , error * perr = nullptr);

    /**
     * Constructs new UDT socket with set `exp_max_counter` to 2 and `exp_threshold` to 625 milliseconds.
     *
     * @param type UDT socket type (only `type_enum::dgram` supported).
     * @param mtu MTU value.
     *
     * @note Descriptions for `exp_max_counter` and `exp_threshold` see in
     *       `udt/newlib/core.hpp`.
     */
    NETTY__EXPORT udt_socket (int mtu, error * perr = nullptr);

    NETTY__EXPORT ~udt_socket ();

    /**
     *  Checks if socket is valid
     */
    NETTY__EXPORT operator bool () const noexcept;

    NETTY__EXPORT socket_id id () const noexcept;

    /**
     * @return Bound address for the listener or listener address for connected socket.
     */
    NETTY__EXPORT socket4_addr saddr () const noexcept;

    NETTY__EXPORT int recv (char * data, int len, error * perr = nullptr);
    NETTY__EXPORT send_result send (char const * data, int len, error * perr = nullptr);

    /**
     * Connects to the UDT server.
     *
     * @return @c conn_status::failure if error occurred while connecting,
     *         @c conn_status::success if connection established successfully or
     *         @c conn_status::in_progress if connection in progress.
     */
    NETTY__EXPORT conn_status connect (socket4_addr const & saddr, error * perr = nullptr);

    /**
     * Shutdown (close) connection.
     */
    NETTY__EXPORT void disconnect (error * perr = nullptr);

    NETTY__EXPORT void dump_options (std::vector<std::pair<std::string, std::string>> & out) const;
};

}} // namespace netty::udt
