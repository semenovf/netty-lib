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
#include "pfs/netty/socket4_addr.hpp"
#include <system_error>

namespace netty {
namespace posix {

/**
 * POSIX inet socket
 */
class inet_socket
{
public:
    using native_type = int;

protected:
    enum class type_enum {
          unknown
        , stream = 0x001
        , dgram  = 0x002
    };

    static native_type constexpr INVALID_SOCKET = -1;

protected:
    native_type _socket {INVALID_SOCKET};

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

    ~inet_socket ();
    inet_socket (inet_socket &&);
    inet_socket & operator = (inet_socket &&);

public:
    /**
     *  Checks if socket is valid
     */
    operator bool () const noexcept;

    native_type native () const noexcept;
    std::streamsize recv (char * data, std::streamsize len);
    std::streamsize send (char const * data, std::streamsize len);
};

}} // namespace netty::posix
