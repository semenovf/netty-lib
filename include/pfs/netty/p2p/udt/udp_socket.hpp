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
#include "pfs/netty/socket4_addr.hpp"
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

    using native_type = UDTSOCKET;

private:
    UDTSOCKET _socket {INVALID_SOCKET};
    socket4_addr _saddr;

public:
    udp_socket () = default;
    NETTY__EXPORT ~udp_socket ();

    udp_socket (udp_socket const &) = delete;
    udp_socket & operator = (udp_socket const &) = delete;

    udp_socket (udp_socket && other) noexcept
    {
        _socket = other._socket;
        _saddr  = other._saddr;
        other._socket = INVALID_SOCKET;
    }

    udp_socket & operator = (udp_socket && other) noexcept
    {
        _socket = other._socket;
        _saddr  = other._saddr;
        other._socket = INVALID_SOCKET;
        return *this;
    }

    native_type native () const
    {
        return _socket;
    }

    NETTY__EXPORT state_enum state () const;

    inet4_addr addr () const noexcept
    {
        return _saddr.addr;
    }

    std::uint16_t port () const noexcept
    {
        return _saddr.port;
    }

    socket4_addr saddr () const noexcept
    {
        return _saddr;
    }

    NETTY__EXPORT udp_socket accept ();
    NETTY__EXPORT void bind (inet4_addr addr, std::uint16_t port);

    inline void bind (socket4_addr saddr)
    {
        bind(saddr.addr, saddr.port);
    }

    NETTY__EXPORT void close ();
    NETTY__EXPORT void connect (inet4_addr addr, std::uint16_t port);
    NETTY__EXPORT void listen (int backlog = 10);
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

namespace fmt {

template <>
struct formatter<netty::p2p::udt::udp_socket>
{
    template <typename ParseContext>
    constexpr auto parse (ParseContext & ctx) -> decltype(ctx.begin())
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format (netty::p2p::udt::udp_socket const & sock
        , FormatContext & ctx) -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "{}.{}"
            , to_string(sock.saddr())
            , sock.native());
    }
};

} // namespace fmt
