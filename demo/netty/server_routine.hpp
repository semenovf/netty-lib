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
#if _MSC_VER
    sprintf_s(hex, sizeof(hex), "%02X", static_cast<std::uint8_t>(buf[0]));
#else
    sprintf(hex, "%02X", static_cast<std::uint8_t>(buf[0]));
#endif
    result += hex;

    for (int i = 1; i < nbytes; i++) {
        result += ' ';
#if _MSC_VER
        sprintf_s(hex, sizeof(hex), "%02X", static_cast<std::uint8_t>(buf[i]));
#else
        sprintf(hex, "%02X", static_cast<std::uint8_t>(buf[i]));
#endif
        result += hex;
    }

    if (nbytes < len)
        result += fmt::format(" ... {} bytes", len - nbytes);

    return result;
}

template <typename ServerTraits>
void start_server (netty::socket4_addr const & saddr)
{
    using socket_id = typename ServerTraits::socket_id;

    LOGD(TAG, "Starting listener on: {}", to_string(saddr));

    std::map<socket_id, typename ServerTraits::socket_type> sockets;

    try {
        netty::property_map_t props;
        typename ServerTraits::listener_type listener {saddr, 10, props};

        auto accept_proc = [& listener, & sockets] (socket_id listener_sock, bool & ok) {
            LOGD(TAG, "Accept client: server socket={}", listener_sock);

            auto client = listener.accept_nonblocking(listener_sock);

            if (client) {
                LOGD(TAG, "Client accepted: socket={}", client.id());

                auto fd = client.id();
                sockets[fd] = std::move(client);
                ok = true;
                return fd;
            }

            ok = false;
            return client.kINVALID_SOCKET;
        };

        typename ServerTraits::poller_type poller {std::move(accept_proc)};

        poller.on_listener_failure = [] (socket_id, netty::error const & err) {
            LOGE(TAG, "Error on server: {}", err.what());
        };

        poller.on_failure = [] (socket_id, netty::error const & err) {
            LOGE(TAG, "Error on peer socket (reader): {}", err.what());
        };

        poller.ready_read = [& sockets] (socket_id sock) {
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

        poller.disconnected = [] (socket_id sock)
        {
            LOGD(TAG, "Disconnected: socket={}", sock);
        };

        std::chrono::milliseconds poller_timeout {1000};

        poller.add_listener(listener);

        while (true) {
            poller.poll(poller_timeout);
        }
    } catch (netty::error const & ex) {
        LOGE(TAG, "ERROR: {}", ex.what());
    }
}
