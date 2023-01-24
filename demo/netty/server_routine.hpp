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

    try {
        ServerType server {saddr, 10};

        typename PollerType::callbacks callbacks;

        callbacks.on_error = [] (typename PollerType::native_socket_type, std::string const & text) {
            LOGE(TAG, "Error on server: {}", text);
        };

        callbacks.accept = [& server, & sockets] (typename PollerType::native_socket_type listener_sock, bool & ok) {
            LOGD(TAG, "Accept client: server socket={}", listener_sock);

            auto pos = sockets.find(listener_sock);

            // Forgot to release entry!!!
            if (pos != sockets.end())
                sockets.erase(pos);

            auto client = server.accept(listener_sock);

            if (client) {
                LOGD(TAG, "Client accepted: socket={}", client.native());

                auto fd = client.native();
                sockets.emplace(client.native(), std::move(client));
                ok = true;
                return fd;
            }

            ok = false;
            return client.INVALID_SOCKET;
        };

        callbacks.disconnected = [] (typename PollerType::native_socket_type sock)
        {
            LOGD(TAG, "Disconnected: socket={}", sock);
        };

        callbacks.ready_read = [& sockets] (typename PollerType::native_socket_type sock) {
            auto pos = sockets.find(sock);

            // Release entry erroneously!!!
            if (pos == sockets.end()) {
                LOGE(TAG, "Release entry erroneously: socket={}", sock);
                return;
            }

            char buf[512];
            std::memset(buf, 0, sizeof(buf));
            auto n = pos->second.recv(buf, sizeof(buf));

            if (n > 0) {
                LOGD(TAG, "Data received: {} bytes: {}", n, buf);
            } else {
                LOGE(TAG, "Data received failure: {}", n);
            }
        };

        PollerType poller{std::move(callbacks)};
        std::chrono::milliseconds poller_timeout {1000};

        poller.add_listener(server);

        while (true)
            poller.poll(poller_timeout);
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}
