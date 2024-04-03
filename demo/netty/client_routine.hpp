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
    bool can_write = true;
    LOGD(TAG, "Starting client");

    SocketType socket;
    PollerType poller;

    poller.on_failure = [& finish] (typename PollerType::native_socket_type, std::string const & text) {
        LOGE(TAG, "Error on client: {}", text);
        finish = true;
    };

    poller.connection_refused = [& finish] (typename PollerType::native_socket_type sock) {
        LOGD(TAG, "Connection refused: socket={}", sock);
        finish = true;
    };

    poller.connected = [] (typename PollerType::native_socket_type sock) {
        LOGD(TAG, "Connected: {}", sock);
    };

    poller.disconnected = [& finish] (typename SocketType::native_type sock) {
        LOGD(TAG, "Disconnected: socket={}", sock);
        finish = true;
    };

    poller.ready_read = [] (typename PollerType::native_socket_type sock) {
        LOGD(TAG, "Ready read");
    };

    poller.can_write = [& can_write] (typename PollerType::native_socket_type sock) {
        can_write = true;
    };

    try {
        std::chrono::milliseconds poller_timeout {100};
        auto conn_state = socket.connect(saddr);
        poller.add(socket, conn_state);

        LOGD(TAG, "Connecting server: {}", to_string(saddr));

        while (!finish) {
            poller.poll(poller_timeout);

            if (can_write) {
                char data[100000] = {'H', 'e', 'l', 'o'};
                auto send_result = socket.send(data, sizeof(data));

                switch (send_result.state) {
                    case netty::send_status::failure:
                        LOGE(TAG, "Send failure: n={}, errno={}", send_result.n, errno);
                        finish = true;
                        break;
                    case netty::send_status::again:
                        LOGD(TAG, "Wait for write: again");
                        can_write = false;
                        poller.wait_for_write(socket);
                        break;
                    case netty::send_status::overflow:
                        LOGD(TAG, "Wait for write: overflow");
                        can_write = false;
                        poller.wait_for_write(socket);
                        break;
                    case netty::send_status::good:
                        // FIXME need to check size of data sent really
                        LOGD(TAG, "Sent: bytes_written={}", send_result.n);
                        break;
                }
            }
        }
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}
