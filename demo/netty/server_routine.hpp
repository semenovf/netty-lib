////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/log.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include <map>

template <typename PollerType, typename ServerType, typename SocketType>
void start_server (netty::socket4_addr const & saddr)
{
    LOGD(TAG, "Starting server on: {}", to_string(saddr));

    std::map<typename SocketType::native_type, SocketType> sockets;
    ServerType server {saddr, 10};

    typename PollerType::callbacks callbacks;

    callbacks.on_error = [] (typename PollerType::native_socket_type, std::string const & text) {
        LOGE(TAG, "Error on server: {}", text);
    };

    callbacks.accept = [& server, & sockets] (typename PollerType::native_socket_type sock) {
        LOGD(TAG, "Accept client: server socket={}", sock);

        auto pos = sockets.find(sock);

        // Forgot to release entry!!!
        if (pos != sockets.end())
            sockets.erase(pos);

        auto client = server.accept(sock);

        if (client) {
            LOGD(TAG, "Client accepted: socket={}", client.native());
            sockets.emplace(sock, std::move(client));
        }

        // char data[4] = {'H', 'e', 'l', 'o'};
        // auto n = sock.send(data, sizeof(data));
        // LOGD(TAG, "-- SEND: n={}, errno={}", n, errno);
    };

//     callbacks.disconnected = [] (typename PollerType::native_socket_type sock) {
//         LOGD(TAG, "Disconnected: socket={}", sock);
//     };

    callbacks.ready_read = [& sockets] (typename PollerType::native_socket_type sock) {
        LOGD(TAG, "Client ready_read");

        auto pos = sockets.find(sock);

        // Release entry erroneously!!!
        if (pos == sockets.end()) {
            LOGE(TAG, "Release entry erroneously: socket={}", sock);
            return;
        }
    };

    callbacks.can_write = [] (typename PollerType::native_socket_type sock) {
        LOGD(TAG, "Client can_write: socket={}", sock);
    };

    try {
        PollerType poller{std::move(callbacks)};
        std::chrono::milliseconds poller_timeout {1000};

        poller.add(server.native());

        while (true) {
            poller.poll(poller_timeout);
        }
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}
