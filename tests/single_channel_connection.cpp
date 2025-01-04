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
#include <pfs/netty/reader_pool.hpp>
#include <pfs/netty/writer_pool.hpp>
#include <pfs/netty/p2p/hello_packet.hpp>
#include <pfs/netty/p2p/primal_serializer.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

static constexpr char const * TAG = "SCC";
static constexpr int MAX_NODES_COUNT = 20;
static constexpr std::uint16_t BASE_PORT = 3101;
static std::atomic<int> s_listener_counter {0};
static std::atomic<int> s_node_counter {0};
static std::atomic<bool> s_is_error {false};

using socket_t = netty::posix::tcp_socket;
using listener_t = netty::posix::tcp_listener;
using socket_id = socket_t::socket_id;
using listener_id = listener_t::listener_id;
using connecting_pool_t = netty::connecting_pool<netty::connecting_epoll_poller_t, socket_t>;
using listener_pool_t = netty::listener_pool<netty::listener_epoll_poller_t, listener_t, socket_t>;
using reader_pool_t = netty::reader_pool<netty::reader_epoll_poller_t, socket_t>;
using writer_pool_t = netty::writer_pool<netty::writer_epoll_poller_t, socket_t>;
using serializer_t = netty::p2p::primal_serializer<pfs::endian::native>;

void worker ()
{
    auto host_id = pfs::generate_uuid();
    netty::error err;

    std::map<socket_id, socket_t> peer_sockets;
    std::map<socket_id, socket_t> connected_sockets;

    int read_counter = 0;
    int write_counter = 0;
    std::uint16_t self_port = BASE_PORT + (s_node_counter++);
    netty::socket4_addr listener_saddr {netty::inet4_addr{127, 0, 0, 1}, self_port};

    listener_pool_t listener_pool;
    connecting_pool_t connecting_pool;
    reader_pool_t reader_pool;
    writer_pool_t writer_pool;

    listener_pool.on_failure([] (netty::error const & err) {
        LOGE(TAG, "listener pool failure: {}", err.what());
    }).on_accepted([& peer_sockets, & reader_pool] (socket_t && sock) {
        auto sock_id = sock.id();
        //LOGD(TAG, "socket accepted: id={}: {}", sock.id(), to_string(sock.saddr()));
        peer_sockets[sock.id()] = std::move(sock);
        reader_pool.add(sock_id);
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

    connecting_pool.on_failure([] (netty::error const & err) {
        LOGE(TAG, "{}", err.what());
    }).on_connected([& connected_sockets, & writer_pool, host_id] (socket_t && sock) {
        auto sock_id = sock.id();
        //LOGD(TAG, "socket connected: id={}: {}", sock.id(), to_string(sock.saddr()));
        connected_sockets[sock.id()] = std::move(sock);

        netty::p2p::hello_packet packet;
        packet.uuid = host_id;
        serializer_t::ostream_type out;
        serializer_t::pack(out, packet);
        writer_pool.enqueue(sock_id, out.data(), out.size());
    }).on_connection_refused ([] (connecting_pool_t * that
            , socket_t && sock, netty::connection_refused_reason reason) {
        LOGE(TAG, "connection refused for socket: id={}: {}: reason: {}, reconnecting", sock.id()
            , to_string(sock.saddr()), to_string(reason));

        std::chrono::seconds timeout {1};
        //LOGD(TAG, "Reconnect after {}", timeout);
        that->connect_timeout(timeout, sock.saddr());
    });

    reader_pool.on_failure([& connected_sockets, & peer_sockets] (socket_id id, netty::error const & err) {
        LOGE(TAG, "read socket failure: id={}: {}", id, err.what());
        connected_sockets.erase(id);
        peer_sockets.erase(id);
    }).on_ready([& read_counter] (socket_id id, std::vector<char> && data) {
        //LOGD(TAG, "Input data ready: id={}: {} bytes", id, data.size());
        read_counter++;
    });

    writer_pool.on_failure([& connected_sockets, & peer_sockets] (socket_id id, netty::error const & err) {
        LOGE(TAG, "write socket failure: id={}: {}", id, err.what());
        connected_sockets.erase(id);
        peer_sockets.erase(id);
    }).on_bytes_written([& write_counter] (socket_id id, std::uint64_t n) {
        //LOGD(TAG, "bytes written: id={}: {}", id, n);
        write_counter++;
    });

    for (auto port = BASE_PORT; port < BASE_PORT + MAX_NODES_COUNT; port++) {
        if (port != self_port) {
            netty::socket4_addr server_addr {{127, 0, 0, 1}, port};
            connecting_pool.connect(server_addr);
        }

        listener_pool.step();
        connecting_pool.step();
    }

    auto n = MAX_NODES_COUNT - 1;

    while (!(read_counter == n && write_counter == n && peer_sockets.size() == n && connected_sockets.size() == n)) {
        std::chrono::milliseconds timeout {5};

        pfs::stopwatch<std::milli> stopwatch;
        listener_pool.step();
        connecting_pool.step();
        writer_pool.step();
        stopwatch.stop();

        timeout -= std::chrono::milliseconds{stopwatch.count()};
        reader_pool.step(timeout);
    }

    writer_pool.step(std::chrono::seconds{1});
    reader_pool.step(std::chrono::seconds{1});
}

TEST_CASE("single channel connection") {
    LOGD(TAG, "s_node_counter.is_lock_free = {}", s_node_counter.is_lock_free());

    std::vector<std::thread> thread_pool {MAX_NODES_COUNT};

    std::for_each(thread_pool.begin(), thread_pool.end(), [] (std::thread & thr) {
        thr = std::thread(worker);
    });

    std::for_each(thread_pool.begin(), thread_pool.end(), [] (std::thread & thr) {
        thr.join();
    });

    CHECK_EQ(s_node_counter.load(), MAX_NODES_COUNT);
}

