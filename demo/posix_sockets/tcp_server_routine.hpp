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
#include "pfs/netty/posix/tcp_server.hpp"
#include <map>

template <typename PollerType>
void start_tcp_server (netty::socket4_addr const & saddr)
{
    LOGD(TAG, "Starting TCP server on: {}", to_string(saddr));

    std::map<netty::posix::tcp_socket::native_type, netty::posix::tcp_socket> sockets;
    netty::posix::tcp_server tcp_server {saddr, 10};

    typename PollerType::callbacks_type callbacks;

    callbacks.on_error = [] (netty::posix::tcp_server::native_type, std::string const & text) {
        LOGE(TAG, "Error on server: {}", text);
    };

    callbacks.accepted = [& sockets] (netty::posix::tcp_socket && sock) {
        LOGD(TAG, "Client accepted: {}", sock.native());

        auto pos = sockets.find(sock.native());

        // Forgot to release entry!!!
        if (pos != sockets.end())
            sockets.erase(pos);

//         char data[4] = {'H', 'e', 'l', 'o'};
//         auto n = sock.send(data, sizeof(data));
//
//         LOGD(TAG, "-- SEND: n={}, errno={}", n, errno);

        sockets.emplace(sock.native(), std::move(sock));
    };

    callbacks.ready_read = [& sockets] (netty::posix::inet_socket::native_type sock) {
        LOGD(TAG, "Client ready_read");

        auto pos = sockets.find(sock);

        // Release entry erroneously!!!
        if (pos == sockets.end()) {
            LOGE(TAG, "Release entry erroneously: {}", sock);
            return;
        }

        char data[2];
        auto n = pos->second.recv(data, sizeof(data));

        LOGD(TAG, "-- RECV: n={}, errno={}, data=[{}{}]", n, errno, data[0], data[1]);
    };

    callbacks.can_write = [] (netty::posix::inet_socket::native_type) {
        LOGD(TAG, "Client can_write");
    };

    try {
        PollerType poller{std::move(callbacks)};
        std::chrono::milliseconds poller_timeout {1000};

        poller.add(tcp_server);

        while (true) {
            poller.poll(poller_timeout);
        }
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}

