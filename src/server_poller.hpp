////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/poller.hpp"

namespace netty {

template <typename Backend, typename ServerType, typename SocketType>
server_poller<Backend, ServerType, SocketType>::server_poller (
    server_poller_callbacks<Backend, SocketType> && callbacks)
    : _listener_poller()
    , _client_poller()
{
    if (callbacks.on_error == nullptr) {
        _listener_poller.on_error = [] (native_socket_type, std::string const & text) {
            fmt::print(stderr, tr::f_("ERROR: server poller error: {}\n"), text);
        };

        _client_poller.on_error = _listener_poller.on_error;
    } else {
        _listener_poller.on_error = callbacks.on_error;
        _client_poller.on_error = std::move(callbacks.on_error);
    }

    if (callbacks.accepted == nullptr)
        accepted = [] (SocketType &&) {};
    else
        accepted = std::move(callbacks.accepted);

    ready_read = std::move(callbacks.ready_read);

    _client_poller.ready_read = [this] (typename ServerType::native_type sock
        , ready_read_flag /*flag*/) {
        if (ready_read)
            ready_read(sock);
    };

    _client_poller.can_write = std::move(callbacks.can_write);

    _listener_poller.ready_read = [this] (typename ServerType::native_type sock
        , ready_read_flag /*flag*/) {
        int n = 0;

        auto pos = _servers.find(sock);

        if (pos == _servers.end()) {
            _listener_poller.on_error(sock, tr::_("server socket not found by native value"));
            return;
        }

        do {
            auto client = pos->second->accept();

            if (!client)
                break;

            _client_poller.add(client.native());
            accepted(std::move(client));
            n++;
        } while (true);
    };
}

template <typename Backend, typename ServerType, typename SocketType>
server_poller<Backend, ServerType, SocketType>::~server_poller () = default;

template <typename Backend, typename ServerType, typename SocketType>
void server_poller<Backend, ServerType, SocketType>::add (ServerType & sock, error * perr)
{
    _listener_poller.add(sock.native(), perr);

    if (perr && !*perr)
        return;

    _servers.emplace(sock.native(), & sock);
}

template <typename Backend, typename ServerType, typename SocketType>
void server_poller<Backend, ServerType, SocketType>::remove (ServerType const & server, error * perr)
{
    _servers.erase(server.native());
    _listener_poller.remove(server.native(), perr);
}

template <typename Backend, typename ServerType, typename SocketType>
int server_poller<Backend, ServerType, SocketType>::poll (std::chrono::milliseconds millis, error * perr)
{
    if (!_listener_poller.empty())
        _listener_poller.poll(millis, perr);

    if (perr && !*perr)
        return _client_poller.poll(millis, perr);

    return -1;
}

} // namespace netty


