////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019 - 2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.12.31 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/error.hpp"
#include "pfs/netty/p2p/sockets_api.hpp"
#include "pfs/netty/p2p/posix/sockets_api.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"

namespace netty {
namespace p2p {

namespace posix {


} // namespace posix

using BACKEND = posix::sockets_api;

static std::int32_t const DEFAULT_LISTENER_BACKLOG {64};
static std::chrono::milliseconds const DEFAULT_POLL_INTERVAL {10};

template <>
sockets_api<BACKEND>::sockets_api ()
{
    _opts.listener_backlog = DEFAULT_LISTENER_BACKLOG;
    _opts.poll_interval = DEFAULT_POLL_INTERVAL;

//     if (_instance_count == 0) {
//         try {
//             UDT::startup_context ctx;
//
//             ctx.state_changed_callback = [this] (socket_id sid) {
//                 _socket_state_changed_buffer.push(sid);
//             };
//
//             UDT::startup(std::move(ctx));
//         } catch (CUDTException ex) {
//             // Here may be exception CUDTException(1, 0, WSAGetLastError());
//             // related to WSAStartup call.
//             if (CUDTException{1, 0, 0}.getErrorCode() == ex.getErrorCode()) {
//                 throw error{make_error_code(errc::system_error)};
//             } else {
//                 throw error{make_error_code(errc::unexpected_error)};
//             }
//         }
//     }
//
//     ++_instance_count;
}

template <>
sockets_api<BACKEND>::~sockets_api ()
{
    for (auto & itpos: _index_by_socket_id) {
        auto pos = itpos.second;
        pos->close();
    }

    _index_by_socket_id.clear();
    _sockets.clear();
}

template <>
void sockets_api<BACKEND>::process_poll_input_event (socket_id sid)
{
    auto * sock = locate(sid);

    // FIXME
    if (sock) {
//         if (sock->state() == udp_socket::LISTENING) {
//             // Accept socket (for UDT backend see udt/api.cpp:440)
//             process_acceptance(sock);
//         } else if (sock->state() == udp_socket::CONNECTED) {
//             if (ready_read)
//                 ready_read(sid, sock);
//         }
    }
}

template <>
void sockets_api<BACKEND>::process_poll_output_event (socket_id sid)
{
    auto * sock = locate(sid);

    // FIXME
    if (sock) {
//         if (sock->state() == udp_socket::LISTENING) {
//             // There is no significant output events for listener (not yet).
//             ;
//         } else if (sock->state() == udp_socket::CONNECTED) {
//             ;
//         }
    }
}

// void sockets_api::process_acceptance (socket_type * plistener)
// {
//     auto sock = plistener->accept();
//     _poller.add(sock, poller_type::POLL_IN_EVENT | poller_type::POLL_ERR_EVENT);
//
//     LOG_TRACE_2("Socket accepted: {}", sock);
//
//     auto options = sock.dump_options();
//
//     for (auto const & opt_item: options) {
//         (void)opt_item;
//         LOG_TRACE_3("   * {}: {}", opt_item.first, opt_item.second);
//     }
//
//     socket_accepted(sock.native(), sock.saddr());
//     add_socket(std::move(sock));
// }
//
// void sockets_api::process_connected (socket_type * psock)
// {
//     LOG_TRACE_2("Socket connected to: {}", *psock);
//
//     auto options = psock->dump_options();
//
//     for (auto const & opt_item: options) {
//         (void)opt_item;
//         LOG_TRACE_3("   * {}: {}", opt_item.first, opt_item.second);
//     }
//
//     socket_connected(psock->native(), psock->saddr());
//     _poller.add(*psock, poller_type::POLL_IN_EVENT | poller_type::POLL_ERR_EVENT);
// }
//
// void sockets_api::process_sockets_state_changed ()
// {
//     if (!_socket_state_changed_buffer.empty()) {
//         socket_id sid;
//
//         if (socket_state_changed) {
//             while (_socket_state_changed_buffer.try_pop(sid)) {
//                 auto pos = _index_by_socket_id.find(sid);
//
//                 if (pos != _index_by_socket_id.end()) {
//                     socket_type * psock = & *pos->second;
//                     socket_state_changed(psock);
//
//                     auto state = psock->state();
//
//                     if (state == udp_socket::CLOSED)
//                         socket_closed(sid, psock->saddr());
//
//                     switch (state) {
//                         case udp_socket::CONNECTING:
//                             LOG_TRACE_2("Connecting in progress to: {}", *psock);
//                             break;
//                         case udp_socket::CONNECTED: {
//                             auto pos1 = _connecting_sockets.find(sid);
//                             bool was_connecting = (pos1 != _connecting_sockets.end());
//
//                             if (was_connecting) {
//                                 LOG_TRACE_2("Connected to: {}", *psock);
//
//                                 process_connected(psock);
//                                 _connecting_sockets.erase(pos1);
//                             } else {
//                                 // Accepted socket
//                                 LOG_TRACE_2("Accepted socket changed to connected state: {}", *psock);
//                             }
//
//                             break;
//                         }
//
//                         case udp_socket::CLOSED:
//                         case udp_socket::BROKEN:
//                         case udp_socket::NONEXIST: {
//                             LOG_TRACE_2("Socket closed: {}", *psock);
//                             _poller.remove(*psock);
//                             _sockets.erase(pos->second);
//                             _index_by_socket_id.erase(pos);
//
//                             auto pos1 = _connecting_sockets.find(sid);
//
//                             if (pos1 != _connecting_sockets.end())
//                                 _connecting_sockets.erase(pos1);
//
//                             break;
//                         }
//
//                         default:
//                             break;
//                     }
//                 }
//             }
//         }
//     }
// }

}} // namespace netty::p2p
