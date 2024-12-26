////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// License: see LICENSE file
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.12.25 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <pfs/log.hpp>
#include <pfs/universal_id.hpp>
#include <pfs/netty/inet4_addr.hpp>
#include <pfs/netty/poller_types.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <pfs/netty/posix/tcp_listener.hpp>
#include <pfs/netty/posix/tcp_socket.hpp>
#include <pfs/netty/connecting_pool.hpp>
#include <pfs/netty/listener_pool.hpp>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <map>
#include <thread>
#include <vector>

static constexpr char const * TAG = "SCC";
static constexpr int MAX_NODES_COUNT = 3;//100;
static constexpr std::uint16_t BASE_PORT = 3101;
static std::atomic<int> s_listener_counter {0};
static std::atomic<int> s_node_counter {0};
static std::atomic<bool> s_is_error {false};

using socket_t = netty::posix::tcp_socket;
using listener_t = netty::posix::tcp_listener;
using connecting_pool_t = netty::connecting_pool<netty::connecting_epoll_poller_t, socket_t>;
using listener_pool_t = netty::listener_pool<netty::listener_epoll_poller_t, listener_t, socket_t>;
using socket_id = listener_pool_t::socket_id;
using listener_id = listener_pool_t::listener_id;

void worker ()
{
    netty::error err;

    std::map<socket_id, socket_t> peer_sockets;
    std::map<socket_id, socket_t> connected_sockets;

    // auto peer_id = pfs::generate_uuid();
    std::uint16_t self_port = BASE_PORT + (s_node_counter++);
    netty::socket4_addr listener_saddr {netty::inet4_addr{127, 0, 0, 1}, self_port};

    listener_pool_t listener_pool;

    listener_pool.on_failure([] (netty::error const & err) {
        LOGE(TAG, "listener pool failure: {}", err.what());
    }).on_accepted([& peer_sockets] (socket_t && sock) {
        // MOVE to writer poller
        LOGD(TAG, "socket accepted: id={}: {}", sock.id(), to_string(sock.saddr()));
        peer_sockets[sock.id()] = std::move(sock);
    });

    listener_pool.add(listener_saddr, & err);

    if (err) {
        if (!s_is_error) {
            s_is_error = true;
            LOGE(TAG, "listener pool failure: {}: {}", to_string(listener_saddr), err.what());
        }

        return;
    }

    listener_pool.listen(MAX_NODES_COUNT * MAX_NODES_COUNT);

    s_listener_counter++;

    // Wait until all threads are initialized
    while (s_listener_counter != MAX_NODES_COUNT)
        ;

    if (s_is_error.load())
        return;

    connecting_pool_t connecting_pool;

    connecting_pool.on_failure([] (netty::error const & err) {
        LOGE(TAG, "{}", err.what());
    }).on_connected([& connected_sockets] (socket_t && sock) {
        LOGD(TAG, "socket connected: id={}: {}", sock.id(), to_string(sock.saddr()));
        connected_sockets[sock.id()] = std::move(sock);
    }).on_connection_refused ([] (socket_t && sock, netty::connection_refused_reason reason) {
        LOGE(TAG, "connection refused for socket: id={}: {}: reason: {}", sock.id()
            , to_string(sock.saddr()), to_string(reason));
    });

    for (auto port = BASE_PORT; port < BASE_PORT + MAX_NODES_COUNT; port++) {
        if (port != self_port) {
            netty::socket4_addr server_addr {{127, 0, 0, 1}, port};
            connecting_pool.connect(server_addr);
        }

        listener_pool.poll();
        connecting_pool.poll();
    }

    auto n = MAX_NODES_COUNT - 1 * 2;

    while (peer_sockets.size() < n && connected_sockets.size() < n) {
        listener_pool.poll(std::chrono::milliseconds{10});
        connecting_pool.poll(std::chrono::milliseconds{10});
    }
}

TEST_CASE("single channel connection") {
    LOGD(TAG, "node_counter.is_lock_free = {}", s_node_counter.is_lock_free());

    std::vector<std::thread> thread_pool {MAX_NODES_COUNT};

    LOGD(TAG, "BEGIN");

    std::for_each(thread_pool.begin(), thread_pool.end(), [] (std::thread & thr) {
        thr = std::thread(worker);
    });

    std::for_each(thread_pool.begin(), thread_pool.end(), [] (std::thread & thr) {
        thr.join();
    });

    LOGD(TAG, "END");

    CHECK_EQ(s_node_counter.load(), MAX_NODES_COUNT);
}

