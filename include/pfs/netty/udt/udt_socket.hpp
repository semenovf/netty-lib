////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.26 Initial version.
//      2023.01.06 Renamed to `udt_socket` and refactored.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/conn_status.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include <chrono>
#include <string>
#include <vector>

namespace netty {
namespace udt {

class basic_udt_server;

struct uninitialized {};

class basic_socket
{
public:
    // Typedef UDTSOCKET as defined in `udt.h`
    using UDTSOCKET = int;

    static UDTSOCKET const INVALID_SOCKET = -1;

protected:
    enum class type_enum {
          unknown
        , stream = 0x001
        , dgram  = 0x002
    };

protected: // static
    static int default_exp_counter ();
    static std::chrono::milliseconds default_exp_threshold ();

public:
    using native_type = UDTSOCKET;

protected:
    native_type _socket {INVALID_SOCKET};

    // Bound address for server.
    // Server address for connected socket.
    socket4_addr _saddr;

protected:
    // Constructs invalid UDT socket
    basic_socket ();

    // 1. Only `type_enum::dgram` supported.
    // 2. Descriptions for `exp_max_counter` and `exp_threshold` see in
    //    `udt/newlib/core.hpp`
    basic_socket (type_enum, int mtu, int exp_max_counter, std::chrono::milliseconds exp_threshold);

    // Constructs UDT socket from native socket (accepted).
    basic_socket (native_type sock, socket4_addr const & saddr);

public:
    ~basic_socket ();

    native_type native () const noexcept;

    std::vector<std::pair<std::string, std::string>> dump_options () const;
};

/**
 * UDT socket
 */
class basic_udt_socket: public basic_socket
{
    friend class basic_udt_server;

protected:
    /**
     * Constructs UDT accepted socket.
     */
    NETTY__EXPORT basic_udt_socket (native_type sock, socket4_addr const & saddr);

    /**
     * Constructs uninitialized (invalid) UDT socket.
     */
    NETTY__EXPORT basic_udt_socket (uninitialized);

    /**
     * Constructs new UDT socket.
     *
     * @param exp_max_counter Max socket expiration counter. Affects the
     *        interval when accepted socket becomes BROKEN. Default is 16.
     * @param exp_threshold Socket (peer, accepted) expiration threshold.
     *        Affects the interval when accepted socket becomes BROKEN
     *        (conjunction with @c exp_max_counter). Default is @c 5 seconds.
     */
    NETTY__EXPORT basic_udt_socket (int mtu, int exp_max_counter
        , std::chrono::milliseconds exp_threshold);

public:
    basic_udt_socket (basic_udt_socket const &) = delete;
    basic_udt_socket & operator = (basic_udt_socket const &) = delete;

    NETTY__EXPORT basic_udt_socket (basic_udt_socket && other);
    NETTY__EXPORT basic_udt_socket & operator = (basic_udt_socket && other);
    NETTY__EXPORT ~basic_udt_socket ();

    /**
     *  Checks if socket is valid
     */
    NETTY__EXPORT operator bool () const noexcept;

    NETTY__EXPORT native_type native () const noexcept;
    NETTY__EXPORT std::streamsize recv (char * data, std::streamsize len);
    NETTY__EXPORT std::streamsize send (char const * data, std::streamsize len);

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
};

template <int MTU = 1500>
class udt_socket: public basic_udt_socket
{
public:
    udt_socket (uninitialized) : basic_udt_socket(uninitialized{})
    {}

    udt_socket (int exp_max_counter = default_exp_counter()
        , std::chrono::milliseconds exp_threshold = default_exp_threshold())
        : basic_udt_socket(MTU, exp_max_counter, exp_threshold)
    {}

    udt_socket (udt_socket &&) = default;
    udt_socket & operator = (udt_socket &&) = default;
    ~udt_socket () = default;
};

}} // namespace netty::udt
