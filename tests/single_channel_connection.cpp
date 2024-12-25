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
// #include <pfs/netty/server_poller.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <pfs/netty/posix/tcp_listener.hpp>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <map>
#include <thread>
#include <vector>

static constexpr char const * TAG = "SCC";
static constexpr int MAX_NODES_COUNT = 100;
static constexpr std::uint16_t BASE_PORT = 3101;
static std::atomic<int> s_node_counter {0};
static std::atomic<bool> s_is_error {false};

using listener_t = netty::posix::tcp_listener;
using listener_poller_t = netty::listener_epoll_poller_t;
using socket_id = listener_poller_t::socket_id;
using listener_id = listener_poller_t::listener_id;

void worker ()
{
    std::map<socket_id, bool> peer_sockets;

    auto peer_id = pfs::generate_uuid();
    std::uint16_t self_port = BASE_PORT + (s_node_counter++);
    netty::socket4_addr listener_saddr {netty::inet4_addr{127, 0, 0, 1}, self_port};

    netty::error err;
    listener_t listener {listener_saddr, & err};

    if (err) {
        if (!s_is_error) {
            s_is_error.store(true);
            LOGE(TAG, "listener failure: {}: {}", to_string(listener_saddr), err.what());
        }

        return;
    }

    // Wait until all threads are initialized
    while (s_node_counter != MAX_NODES_COUNT)
        ;

    if (s_is_error.load())
        return;

    // auto accept_proc = [& listener] (listener_id listener_sock, bool & success) {
    //     netty::error err;
    //     auto sock = listener.accept_nonblocking(listener_sock, & err);
    //     success = !err;
    //
    //     if (err)
    //         LOGE(TAG, "Accept failure: {}", err.what());
    //
    //     return sock;
    // };

    listener_poller_t listener_poller;
    listener_poller.on_failure = [] (listener_id listener_sock, netty::error const & err) {
        LOGE(TAG, "Listener failure (id={}): {}", listener_sock, err.what());
    };

    listener_poller.accept = [] (listener_id listener_sock) {
        netty::error err;
        auto peer_socket = listener_t::accept_nonblocking(listener_sock, & err);

        //bool ok = true;
        // auto sock = this->accept(listener_sock, ok);
        //
        // if (ok) {
        //     accepted(sock);
        //     _addable_readers.push_back(sock);
        // }
    };

    listener_poller.add(listener.id());

    for (auto p = BASE_PORT; p < BASE_PORT + MAX_NODES_COUNT; p++) {
        if (p != self_port) {

        }
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

