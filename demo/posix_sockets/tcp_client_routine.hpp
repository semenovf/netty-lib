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

#include <sys/socket.h>

template <typename PollerType>
void start_tcp_client (netty::socket4_addr const & saddr)
{
    int counter = 5;
    bool finish = false;

    LOGD(TAG, "Starting TCP client");

    typename PollerType::callbacks_type callbacks;
    netty::posix::tcp_socket tcp_socket;

    callbacks.on_error = [& finish] (netty::posix::tcp_server::native_type, std::string const & text) {
        LOGE(TAG, "Error on client: {}", text);
        finish = true;
    };

    callbacks.connection_refused = [& finish] (netty::posix::tcp_socket::native_type sock) {
        LOGD(TAG, "Connection refused: socket={}", sock);
        finish = true;
    };

    callbacks.connected = [] (netty::posix::tcp_socket::native_type sock) {
        LOGD(TAG, "Connected: {}", sock);

//         char buf[1];
//         auto n = ::recv(sock, buf, sizeof(buf), MSG_PEEK | MSG_DONTWAIT);
//
//         LOGD(TAG, "-- CONNECTED: recv: n={}, error={}", n, errno);
    };

    callbacks.disconnected = [& finish] (netty::posix::tcp_socket::native_type sock) {
        LOGD(TAG, "Disconnected: socket={}", sock);
        finish = true;
    };

    callbacks.ready_read = [& tcp_socket, & counter] (netty::posix::tcp_socket::native_type sock) {
        LOGD(TAG, "Client ready_read");

        char data[2];
        auto n = tcp_socket.recv(data, sizeof(data));

        LOGD(TAG, "-- RECV: n={}, errno={}, data=[{}{}]", n, errno, data[0], data[1]);

//         int error_val = 0;
//         socklen_t len = sizeof(error_val);
//         auto rc = getsockopt(sock, SOL_SOCKET, SO_ERROR, & error_val, & len);

//         LOGD(TAG, "-- READY_READ: getsockopt: rc={}, SO_ERROR={}", rc, error_val);
//
//         char buf[1];
//         auto n = ::recv(sock, buf, 1, MSG_PEEK);
//
//         LOGD(TAG, "-- READY_READ: recv: n={}, error={}", n, errno);
        --counter;
    };

    callbacks.can_write = [& counter] (netty::posix::tcp_socket::native_type sock) {
//         LOGD(TAG, "Client can_write");

//         int error_val = 0;
//         socklen_t len = sizeof(error_val);
//         auto rc = getsockopt(sock, SOL_SOCKET, SO_ERROR, & error_val, & len);

//         LOGD(TAG, "-- CAN_WRITE: getsockopt: rc={}, SO_ERROR={}", rc, error_val);

        --counter;
    };

    try {
        PollerType poller {std::move(callbacks)};
        std::chrono::milliseconds poller_timeout {1000};

        poller.add(tcp_socket);

        LOGD(TAG, "Connecting server: {}", to_string(saddr));

        tcp_socket.connect(saddr);

        while (!finish) {
            poller.poll(poller_timeout);

//             if (counter == 0) {
//                 LOGD(TAG, "Shutting down ...");
//                 tcp_socket.disconnect();
//             }
        }
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}

