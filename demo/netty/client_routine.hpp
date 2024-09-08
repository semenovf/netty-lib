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
#include <numeric>

template <typename ClientTraits>
void start_client (netty::socket4_addr const & saddr)
{
    using native_socket_type = typename ClientTraits::native_socket_type;

    bool finish = false;
    bool can_write = false;
    LOGD(TAG, "Starting client");

    netty::property_map_t props; // Use default properties
    typename ClientTraits::socket_type socket {props};
    typename ClientTraits::poller_type poller;

    poller.on_failure = [& finish] (native_socket_type, netty::error const & err) {
        LOGE(TAG, "Error on client: {}", err.what());
        finish = true;
    };

    poller.connection_refused = [& finish] (native_socket_type sock, bool /*timedout*/) {
        LOGD(TAG, "Connection refused: socket={}", sock);
        finish = true;
    };

    poller.connected = [& can_write] (native_socket_type sock) {
        LOGD(TAG, "Connected: {}", sock);
        can_write = true;
    };

    poller.disconnected = [& finish, & can_write] (native_socket_type sock) {
        LOGD(TAG, "Disconnected: socket={}", sock);
        finish = true;
        can_write = false;
    };

    poller.ready_read = [] (native_socket_type sock) {
        LOGD(TAG, "Ready read");
    };

    poller.can_write = [& can_write] (native_socket_type sock) {
        can_write = true;
    };

    try {
        std::chrono::milliseconds poller_timeout {100};
        auto conn_state = socket.connect(saddr);
        poller.add(socket, conn_state);

        LOGD(TAG, "Connecting server: {}", to_string(saddr));

        std::uint8_t counter_initial = 0;

        while (!finish) {
            poller.poll(poller_timeout);

            if (can_write) {
                char data[100000] = {'H', 'e', 'l', 'o'};

                std::iota(data + 4, data + 100000, counter_initial++);

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
                    case netty::send_status::network:
                        LOGD(TAG, "Network failure");
                        finish = true;
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
