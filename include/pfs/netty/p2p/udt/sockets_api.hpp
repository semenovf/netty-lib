////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.11.06 Initial version.
//      2022.08.15 Changed signatures for startup/cleanup.
//      2022.09.?? Renamed to `engine` and implementing full functional backend
//                 for basic network operations over UDT.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "poller.hpp"
#include "udp_socket.hpp"
#include "pfs/ring_buffer.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <set>
#include <unordered_map>

namespace netty {
namespace p2p {
namespace udt {

class sockets_api final
{
public:
    using poller_type = poller;
    using socket_type = udp_socket;
    using socket_id   = socket_type::native_type;

    enum option_enum: std::int16_t
    {
          listener_address   // socket4_addr

        // The maximum length to which the queue of pending connections
        // for listener may grow.
        , listener_backlog   // std::int32_t

        , poll_interval      // std::chrono::milliseconds
    };

private:
    using socket_container = std::list<socket_type>;
    using socket_iterator  = typename socket_container::iterator;

private:
    struct options {
        socket4_addr listener_address;
        std::int32_t listener_backlog;
        std::chrono::milliseconds poll_interval;
    } _opts;

    // All sockets (listeners / readers / writers)
    socket_container _sockets;

    // Mapping socket identified by key (native handler) to socket iterator
    // (see _sockets)
    std::unordered_map<socket_id, socket_iterator> _index_by_socket_id;

    std::set<socket_id> _connecting_sockets;

    poller_type _poller;

    pfs::ring_buffer_mt<socket_id, 256> _socket_state_changed_buffer;

private: // static
    static std::atomic<int> _instance_count;

public:
    mutable std::function<void (std::string const &)> log_error
        = [] (std::string const &) {};

    mutable std::function<void (socket_type const *)> socket_state_changed
        = [] (socket_type const *) {};

    mutable std::function<void (socket_id, socket4_addr)> socket_accepted
        = [] (socket_id, socket4_addr) {};

    mutable std::function<void (socket_id, socket4_addr)> socket_connected
        = [] (socket_id, socket4_addr) {};

    mutable std::function<void (socket_id, socket4_addr)> socket_closed
        = [] (socket_id, socket4_addr) {};

    mutable std::function<void (socket_id, socket_type *)> ready_read
        = [] (socket_id, socket_type *) {};

private:
    sockets_api (sockets_api const &) = delete;
    sockets_api (sockets_api &&) = delete;
    sockets_api & operator = (sockets_api const &) = delete;
    sockets_api & operator = (sockets_api &&) = delete;

    socket_id add_socket (socket_type && sock);
    void poll (std::chrono::milliseconds interval);
    void process_sockets_state_changed ();
    void process_poll_input_event (socket_id sid);
    void process_poll_output_event (socket_id sid);
    void process_acceptance (socket_type * plistener);
    void process_connected (socket_type * psock);

public:
    NETTY__EXPORT sockets_api ();
    NETTY__EXPORT ~sockets_api ();

    /**
     * Sets boolean or integer type option.
     *
     * @return @c true on success, or @c false if option is unsuitable or
     *         value is bad.
     */
    NETTY__EXPORT bool set_option (option_enum opttype, std::intmax_t value);

    /**
     * Sets socket address type option.
     */
    NETTY__EXPORT bool set_option (option_enum opttype, socket4_addr sa);

    /**
     * Sets chrono type option.
     */
    NETTY__EXPORT bool set_option (option_enum opttype, std::chrono::milliseconds msecs);

    NETTY__EXPORT socket_type * locate (socket_id sid) const;
    NETTY__EXPORT socket_id listen ();
    NETTY__EXPORT socket_id connect (inet4_addr addr, std::uint16_t port);

    inline socket_id connect (socket4_addr saddr)
    {
        return connect(saddr.addr, saddr.port);
    }

    NETTY__EXPORT void loop ();
};

}}} // namespace netty::p2p::udt
