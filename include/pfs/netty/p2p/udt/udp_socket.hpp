////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/exports.hpp"
#include "pfs/netty/inet4_addr.hpp"
#include "pfs/fmt.hpp"
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace netty {
namespace p2p {
namespace udt {

class udp_socket
{
    // Typedef UDTSOCKET as defined in `udt.h`
    using UDTSOCKET = int;

    static constexpr UDTSOCKET INVALID_SOCKET = -1;

public:
    // Must be same as UDTSTATUS defined in `udt.h`
    enum state_enum {
          INIT = 1
        , OPENED
        , LISTENING
        , CONNECTING
        , CONNECTED
        , BROKEN
        , CLOSING
        , CLOSED
        , NONEXIST
    };

    using id_type = UDTSOCKET;

private:
    UDTSOCKET _socket {INVALID_SOCKET};

public:
    mutable std::function<void (std::string const &)> failure;

public:
    udp_socket () = default;
    NETTY__EXPORT ~udp_socket ();

    udp_socket (udp_socket const &) = delete;
    udp_socket & operator = (udp_socket const &) = delete;

    udp_socket (udp_socket && other) noexcept
    {
        _socket = other._socket;
        other._socket = INVALID_SOCKET;
    }

    udp_socket & operator = (udp_socket && other) noexcept
    {
        _socket = other._socket;
        other._socket = INVALID_SOCKET;
        return *this;
    }

    id_type id () const
    {
        return _socket;
    }

    NETTY__EXPORT state_enum state () const;

    std::string backend_string () const noexcept
    {
        return "UDT";
    }

    NETTY__EXPORT udp_socket accept (inet4_addr * addr, std::uint16_t * port);
    NETTY__EXPORT bool bind (inet4_addr const & addr, std::uint16_t port);
    NETTY__EXPORT void close ();
    NETTY__EXPORT bool connect (inet4_addr const & addr, std::uint16_t port);
    NETTY__EXPORT bool listen (int backlog = 10);
    NETTY__EXPORT std::streamsize recvmsg (char * msg, std::streamsize len);
    NETTY__EXPORT std::streamsize sendmsg (char const * data, std::streamsize len);

    NETTY__EXPORT std::string error_string () const noexcept;
    NETTY__EXPORT int error_code () const noexcept;

    /**
     * Checks if the output buffer is overflow.
     */
    NETTY__EXPORT bool overflow () const noexcept;

    inline std::string state_string () const noexcept
    {
        return state_string(state());
    }

    NETTY__EXPORT std::vector<std::pair<std::string, std::string>> dump_options () const;

public: // static
    static NETTY__EXPORT std::string state_string (int state);
};

}}} // namespace netty::p2p::udt
