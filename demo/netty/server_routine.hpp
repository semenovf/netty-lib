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
#include <cstdio>
#include <map>

std::string stringify_bytes (char const * buf, int len, int nbytes)
{
    if (nbytes > len)
        nbytes = len;

    if (nbytes == 0)
        return std::string{};

    std::string result;
    result.reserve(nbytes * 3);

    char hex[3];
    sprintf(hex, "%02X", static_cast<std::uint8_t>(buf[0]));
    result += hex;

    for (int i = 1; i < nbytes; i++) {
        result += ' ';
        sprintf(hex, "%02X", static_cast<std::uint8_t>(buf[i]));
        result += hex;
    }

    if (nbytes < len) {
        result += fmt::format(" ... {} bytes", len - nbytes);
    }

    return result;
}

template <typename PollerType, typename ServerType, typename SocketType>
void start_server (netty::socket4_addr const & saddr)
{
    LOGD(TAG, "Starting server on: {}", to_string(saddr));

    std::map<typename SocketType::native_type, SocketType> sockets;

    try {
        ServerType server {saddr, 10};

        auto accept_proc = [& server, & sockets] (typename PollerType::native_socket_type listener_sock, bool & ok) {
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
            return client.kINVALID_SOCKET;
        };

        PollerType poller{std::move(accept_proc)};
        poller.on_listener_failure = [] (typename PollerType::native_socket_type, netty::error const & err) {
            LOGE(TAG, "Error on server: {}", err.what());
        };

        poller.on_failure = [] (typename PollerType::native_socket_type, netty::error const & err) {
            LOGE(TAG, "Error on peer socket (reader): {}", err.what());
        };

        poller.ready_read = [& sockets] (typename PollerType::native_socket_type sock) {
            auto pos = sockets.find(sock);

            // Release entry erroneously!!!
            if (pos == sockets.end()) {
                LOGE(TAG, "Release entry erroneously: socket={}", sock);
                return;
            }

            char buf[512];
            std::memset(buf, 0, sizeof(buf));
            int n = 0;

            while ((n = pos->second.recv(buf, sizeof(buf))) > 0) {
                LOGD(TAG, "Data received: {} bytes: {}", n, stringify_bytes(buf, n, 10));
            }

            if (n < 0) {
                LOGE(TAG, "Receive data failure: {}", n);
            }
        };

        poller.disconnected = [] (typename PollerType::native_socket_type sock)
        {
            LOGD(TAG, "Disconnected: socket={}", sock);
        };

        std::chrono::milliseconds poller_timeout {1000};

        poller.add_listener(server);

        while (true) {
            poller.poll(poller_timeout);
        }
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}
