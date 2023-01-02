////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.12.30 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "poller.hpp"
#include "tcp_socket.hpp"
// #include "pfs/ring_buffer.hpp"
// #include "pfs/netty/exports.hpp"
// #include "pfs/netty/socket4_addr.hpp"
// #include <atomic>
// #include <chrono>
// #include <functional>
// #include <list>
// #include <set>
// #include <unordered_map>

namespace netty {
namespace p2p {
namespace posix {

class sockets_api
{
public:
    using poller_type = poller;
    using socket_type = tcp_socket;
    using socket_id   = socket_type::native_type;

//     enum option_enum: std::int16_t
//     {
//           listener_address   // socket4_addr
//
//         // The maximum length to which the queue of pending connections
//         // for listener may grow.
//         , listener_backlog   // std::int32_t
//
//         , poll_interval      // std::chrono::milliseconds
//     };
//
// private:
//     using socket_container = std::list<socket_type>;
//     using socket_iterator  = typename socket_container::iterator;
//
// private:
//     struct options {
//         socket4_addr listener_address;
//         std::int32_t listener_backlog;
//         std::chrono::milliseconds poll_interval;
//     } _opts;
//
//     // All sockets (listeners / readers / writers)
//     socket_container _sockets;
//
//     // Mapping socket identified by key (native handler) to socket iterator
//     // (see _sockets)
//     std::unordered_map<socket_id, socket_iterator> _index_by_socket_id;
//
//     // FIXME
// //     std::set<socket_id> _connecting_sockets;



//     pfs::ring_buffer_mt<socket_id, 256> _socket_state_changed_buffer;
//
// private: // static
//     static std::atomic<int> _instance_count;
//
// private:
//     sockets_api (sockets_api const &) = delete;
//     sockets_api (sockets_api &&) = delete;
//     sockets_api & operator = (sockets_api const &) = delete;
//     sockets_api & operator = (sockets_api &&) = delete;
//
//     // FIXME
// //     socket_id add_socket (socket_type && sock);
// //     void poll (std::chrono::milliseconds interval);
// //     void process_sockets_state_changed ();
// //     void process_poll_input_event (socket_id sid);
// //     void process_poll_output_event (socket_id sid);
// //     void process_acceptance (socket_type * plistener);
// //     void process_connected (socket_type * psock);
//
// public:
//     NETTY__EXPORT sockets_api ();
//     NETTY__EXPORT ~sockets_api ();
};

}}} // namespace netty::p2p::posix
