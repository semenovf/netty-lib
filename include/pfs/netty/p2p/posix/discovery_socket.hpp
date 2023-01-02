////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.21 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/fmt.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include <functional>
#include <string>

namespace netty {
namespace p2p {
namespace posix {

class discovery_socket
{
    enum class muticast_group_op { join, leave };

public:
    using native_type = int;

    static native_type constexpr INVALID_SOCKET = -1;

//     // Must be same as `QAbstractSocket::SocketState`
//     enum state_enum {
//           UNCONNECTED = 0
//         , HOSTLOOKUP
//         , CONNECTING
//         , CONNECTED
//         , BOUND
//         , CLOSING
//         , LISTENING
//     };
//
//     using native_type = qintptr; // See QAbstractSocket::socketDescriptor()

private:
    native_type _socket {INVALID_SOCKET};

public:
    // Discovery data ready
    mutable std::function<void (socket4_addr, char const *, std::size_t)> data_ready
        = [] (socket4_addr, char const *, std::size_t) {};

private:
//     // If `addr` is empty select first running and non-loopback interface
//     QNetworkInterface iface_by_address (std::string const & addr);

    void process_multicast_group (inet4_addr addr
        , std::string const & network_iface
        , muticast_group_op op);

private:
    discovery_socket (discovery_socket const &) = delete;
    discovery_socket & operator = (discovery_socket const &) = delete;
    discovery_socket (discovery_socket &&) = delete;
    discovery_socket & operator = (discovery_socket &&) = delete;

public:
    NETTY__EXPORT discovery_socket (socket4_addr const & saddr);
    NETTY__EXPORT ~discovery_socket ();

    native_type native () const
    {
        return _socket;
    }

    NETTY__EXPORT inet4_addr addr () const noexcept;
    NETTY__EXPORT std::uint16_t port () const noexcept;
    NETTY__EXPORT socket4_addr saddr () const noexcept;

//     inline state_enum state () const
//     {
//         return static_cast<state_enum>(_socket.state());
//     }
//
//     inline void join_multicast_group (inet4_addr addr
//         , std::string const & network_iface = "")
//     {
//         process_multicast_group(addr, network_iface, muticast_group_op::join);
//     }
//
//     inline void leave_multicast_group (inet4_addr addr
//         , std::string const & network_iface = "")
//     {
//         process_multicast_group(addr, network_iface, muticast_group_op::leave);
//     }
//
//     inline bool has_pending_data () const
//     {
//         return _socket.hasPendingDatagrams();
//     }
//
//     NETTY__EXPORT void process_incoming_data ();
//
//     inline std::string error_string () const
//     {
//         return (_socket.error())
//             ? _socket.errorString().toStdString()
//             : std::string{};
//     }
//
//     inline std::string state_string () const noexcept
//     {
//         return state_string(state());
//     }
//
//     NETTY__EXPORT void bind (socket4_addr saddr);
//     NETTY__EXPORT std::streamsize send (char const * data, std::streamsize size
//         , socket4_addr saddr);
//
//     static NETTY__EXPORT std::string state_string (state_enum status);
};

}}} // namespace netty::p2p::posix

namespace fmt {

template <>
struct formatter<netty::p2p::posix::discovery_socket>
{
    template <typename ParseContext>
    constexpr auto parse (ParseContext & ctx) -> decltype(ctx.begin())
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format (netty::p2p::posix::discovery_socket const & sock
        , FormatContext & ctx) -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "{}.{}", to_string(sock.saddr())
            , sock.native());
    }
};

} // namespace fmt

