////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/inet4_addr.hpp"
#include "pfs/netty/posix/tcp_socket.hpp"
#include "pfs/netty/udt/udt_socket.hpp"
// #include <thread>
#include <sys/socket.h>

template <typename PollerType, typename SocketType>
void start_client (netty::socket4_addr const & saddr)
{
    bool finish = false;

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

//     callbacks.disconnected = [& finish] (typename SocketType::native_type sock) {
//         LOGD(TAG, "Disconnected: socket={}", sock);
//         finish = true;
//     };
//
    callbacks.ready_read = [/*& socket, & counter*/] (typename PollerType::native_socket_type sock) {
        LOGD(TAG, "Client ready_read");

//         char data[2];
//         auto n = socket.recv(data, sizeof(data));
//
//         LOGD(TAG, "-- RECV: n={}, errno={}, data=[{}{}]", n, errno, data[0], data[1]);
//
// //         int error_val = 0;
// //         socklen_t len = sizeof(error_val);
// //         auto rc = getsockopt(sock, SOL_SOCKET, SO_ERROR, & error_val, & len);
//
// //         LOGD(TAG, "-- READY_READ: getsockopt: rc={}, SO_ERROR={}", rc, error_val);
// //
// //         char buf[1];
// //         auto n = ::recv(sock, buf, 1, MSG_PEEK);
// //
// //         LOGD(TAG, "-- READY_READ: recv: n={}, error={}", n, errno);
//         --counter;
    };

    callbacks.can_write = [/*& counter*/] (typename PollerType::native_socket_type sock) {
//         LOGD(TAG, "Client can_write");

//         int error_val = 0;
//         socklen_t len = sizeof(error_val);
//         auto rc = getsockopt(sock, SOL_SOCKET, SO_ERROR, & error_val, & len);
//         LOGD(TAG, "-- CAN_WRITE: getsockopt: rc={}, SO_ERROR={}", rc, error_val);

//         --counter;
    };

    try {
        PollerType poller {std::move(callbacks)};
        std::chrono::milliseconds poller_timeout {1000};
        auto conn_state = socket.connect(saddr);
        poller.add(socket.native(), conn_state);

        LOGD(TAG, "Connecting server: {}", to_string(saddr));

        while (!finish) {
            poller.poll(poller_timeout);

//             std::this_thread::sleep_for(std::chrono::seconds{5});

            char data[4] = {'H', 'e', 'l', 'o'};
            auto n = socket.send(data, sizeof(data));

            if (n < 0) {
                break;
            }

            LOGD(TAG, "-- SEND: n={}, errno={}", n, errno);

            // //         char buf[1];
// //         auto n = ::recv(sock, buf, 1, MSG_PEEK);

//             if (counter == 0) {
//                 LOGD(TAG, "Shutting down ...");
//                 tcp_socket.disconnect();
//             }
        }
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}
