////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/fmt.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/poller.hpp"
#include <sys/types.h>
#include <sys/socket.h>

namespace netty {

template <typename Backend>
client_poller<Backend>::client_poller (client_poller_callbacks<Backend> && callbacks)
    : _connecting_poller()
    , _regular_poller()
{
    if (callbacks.on_error == nullptr) {
        _connecting_poller.on_error = [] (native_socket_type, std::string const & text) {
            fmt::print(stderr, tr::f_("ERROR: client poller error: {}\n"), text);
        };

        _regular_poller.on_error = _connecting_poller.on_error;
    } else {
        _connecting_poller.on_error = callbacks.on_error;
        _regular_poller.on_error = std::move(callbacks.on_error);
    }

    if (callbacks.connection_refused == nullptr)
        _connecting_poller.connection_refused = [] (native_socket_type) {};
    else
        _connecting_poller.connection_refused = std::move(callbacks.connection_refused);

    if (callbacks.connected == nullptr)
        connected = [] (native_socket_type) {};
    else
        connected = std::move(callbacks.connected);

    if (callbacks.disconnected == nullptr)
        disconnected = [] (native_socket_type) {};
    else
        disconnected = std::move(callbacks.disconnected);

    ready_read = std::move(callbacks.ready_read);

    _regular_poller.can_write = std::move(callbacks.can_write);

    _connecting_poller.can_write = [this] (native_socket_type sock) {
        _connecting_poller.remove(sock);
        _regular_poller.add(sock);
        connected(sock);
    };

    _regular_poller.ready_read = [this] (native_socket_type sock, ready_read_flag flag) {
        if (flag == ready_read_flag::good) {
            if (ready_read)
                ready_read(sock);
            return;
        }

        bool disconnect = false;

        if (flag == ready_read_flag::disconnected) { // from `epoll_poller` and `poll_poller`
            disconnect = true;
        } else if (flag == ready_read_flag::check_disconnected) { // from `select_poller`
            char buf[1];
            auto n = ::recv(sock, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);

            if (n < 0) {
                _regular_poller.on_error(sock, tr::f_("read socket failure: {}"
                    , pfs::system_error_text(errno)));
                disconnect = true;
            } else if (n == 0) {
                disconnect = true;
            } else {
                if (ready_read)
                    ready_read(sock);
            }
        }

        if (disconnect) {
            _regular_poller.remove(sock);
            disconnected(sock);
        }
    };
}

template <typename Backend>
client_poller<Backend>::~client_poller () = default;

template <typename Backend>
void client_poller<Backend>::add (posix::tcp_socket & sock, error * perr)
{
    _connecting_poller.add(sock.native(), perr);
}

template <typename Backend>
void client_poller<Backend>::add (posix::udp_socket & sock, error * perr)
{
    _regular_poller.add(sock.native(), perr);
}

template <typename Backend>
void client_poller<Backend>::remove (posix::tcp_socket const & sock, error * perr)
{
    _connecting_poller.remove(sock.native(), perr);
    _regular_poller.remove(sock.native(), perr);
}

template <typename Backend>
void client_poller<Backend>::remove (posix::udp_socket const & sock, error * perr)
{
    _connecting_poller.remove(sock.native(), perr);
    _regular_poller.remove(sock.native(), perr);
}

template <typename Backend>
int client_poller<Backend>::poll (std::chrono::milliseconds millis, error * perr)
{
    if (!_connecting_poller.empty())
        _connecting_poller.poll(std::chrono::milliseconds{0}, perr);

    if (perr && !*perr)
        return -1;

    return _regular_poller.poll(millis, perr);
}

} // namespace netty

