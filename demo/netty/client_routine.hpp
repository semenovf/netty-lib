////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/socket4_addr.hpp"

template <typename PollerType, typename SocketType>
void start_client (netty::socket4_addr const & saddr)
{
    bool finish = false;
    bool can_write = false;
    LOGD(TAG, "Starting client");

    typename PollerType::callbacks callbacks;
    SocketType socket;

    callbacks.on_error = [& finish] (typename PollerType::native_socket_type, std::string const & text) {
        LOGE(TAG, "Error on client: {}", text);
        finish = true;
    };

    callbacks.connection_refused = [& finish] (typename PollerType::native_socket_type sock) {
        LOGD(TAG, "Connection refused: socket={}", sock);
        finish = true;
    };

    callbacks.connected = [] (typename PollerType::native_socket_type sock) {
        LOGD(TAG, "Connected: {}", sock);

//         char buf[1];
//         auto n = ::recv(sock, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);
//
//         LOGD(TAG, "-- CONNECTED: recv: n={}, error={}", n, errno);
    };

    callbacks.disconnected = [& finish] (typename SocketType::native_type sock) {
        LOGD(TAG, "Disconnected: socket={}", sock);
        finish = true;
    };

    callbacks.ready_read = [] (typename PollerType::native_socket_type sock) {
        LOGD(TAG, "Ready read");
    };

    callbacks.can_write = [& can_write] (typename PollerType::native_socket_type sock) {
        can_write = true;
    };

    try {
        PollerType poller {std::move(callbacks)};
        std::chrono::milliseconds poller_timeout {100};
        auto conn_state = socket.connect(saddr);
        poller.add(socket, conn_state);

        LOGD(TAG, "Connecting server: {}", to_string(saddr));

        while (!finish) {
            poller.poll(poller_timeout);

            if (can_write) {
                can_write = false;
                char data[4] = {'H', 'e', 'l', 'o'};
                auto n = socket.send(data, sizeof(data));

                if (n < 0) {
                    LOGE(TAG, "Send failure: n={}, errno={}", n, errno);
                    break;
                }

                LOGD(TAG, "SEND: n={}", n);
            } else {
                LOGW(TAG, "Cannot write");
            }
        }
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}
