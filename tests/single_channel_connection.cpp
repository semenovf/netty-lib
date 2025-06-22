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
#include <pfs/netty/startup.hpp>
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

#if NETTY__UDT_ENABLED
#   include <pfs/netty/udt/udt_listener.hpp>
#   include <pfs/netty/udt/udt_socket.hpp>
#endif

// Before #include "tools.hpp"
static constexpr char const * TAG = "SCC";

#include "tools.hpp"

static constexpr int MAX_NODES_COUNT = 20;
static constexpr std::uint16_t BASE_PORT = 3101;
static std::atomic<int> s_listener_counter {0};
static std::atomic<int> s_node_counter {0};
static std::atomic<int> s_node_reverse_counter {MAX_NODES_COUNT};
static std::atomic<bool> s_is_error {false};

#if defined(NETTY__SCC_TEST_EPOLL) && defined(NETTY__EPOLL_ENABLED)
using socket_t = netty::posix::tcp_socket;
using listener_t = netty::posix::tcp_listener;
using connecting_pool_t = netty::connecting_pool<socket_t, netty::connecting_epoll_poller_t>;
using listener_pool_t = netty::listener_pool<listener_t, socket_t, netty::listener_epoll_poller_t>;
using reader_pool_t = netty::reader_pool<socket_t, netty::reader_epoll_poller_t>;
using writer_pool_t = netty::writer_pool<socket_t, netty::writer_epoll_poller_t>;
#elif defined(NETTY__SCC_TEST_POLL) && defined(NETTY__POLL_ENABLED)
using socket_t = netty::posix::tcp_socket;
using listener_t = netty::posix::tcp_listener;
using connecting_pool_t = netty::connecting_pool<socket_t, netty::connecting_poll_poller_t>;
using listener_pool_t = netty::listener_pool<listener_t, socket_t, netty::listener_poll_poller_t>;
using reader_pool_t = netty::reader_pool<socket_t, netty::reader_poll_poller_t>;
using writer_pool_t = netty::writer_pool<socket_t, netty::writer_poll_poller_t>;
#elif defined(NETTY__SCC_TEST_SELECT) && defined(NETTY__SELECT_ENABLED)
using socket_t = netty::posix::tcp_socket;
using listener_t = netty::posix::tcp_listener;
using connecting_pool_t = netty::connecting_pool<socket_t, netty::connecting_select_poller_t>;
using listener_pool_t = netty::listener_pool<listener_t, socket_t, netty::listener_select_poller_t>;
using reader_pool_t = netty::reader_pool<socket_t, netty::reader_select_poller_t>;
using writer_pool_t = netty::writer_pool<socket_t, netty::writer_select_poller_t>;
#elif defined(NETTY__SCC_TEST_UDT) && defined(NETTY__UDT_ENABLED)
using socket_t = netty::udt::udt_socket;
using listener_t = netty::udt::udt_listener;
using connecting_pool_t = netty::connecting_pool<socket_t, netty::connecting_udt_poller_t>;
using listener_pool_t = netty::listener_pool<listener_t, socket_t, netty::listener_udt_poller_t>;
using reader_pool_t = netty::reader_pool<socket_t, netty::reader_udt_poller_t>;
using writer_pool_t = netty::writer_pool<socket_t, netty::writer_udt_poller_t>;
#elif defined(NETTY__SCC_TEST_ENET) && defined(NETTY__ENET_ENABLED)
using socket_t = netty::enet::enet_socket;
using listener_t = netty::enet::enet_listener;
using connecting_pool_t = netty::connecting_pool<socket_t, netty::connecting_enet_poller_t>;
using listener_pool_t = netty::listener_pool<listener_t, socket_t, netty::listener_enet_poller_t>;
using reader_pool_t = netty::reader_pool<socket_t, netty::reader_enet_poller_t>;
using writer_pool_t = netty::writer_pool<socket_t, netty::writer_enet_poller_t>;
#endif

using socket_id = socket_t::socket_id;
using listener_id = listener_t::listener_id;

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

    LOGD(TAG, "{:04}: START", self_port);

    listener_pool_t listener_pool;
    connecting_pool_t connecting_pool;
    reader_pool_t reader_pool;
    writer_pool_t writer_pool;

    listener_pool.on_failure = [self_port] (netty::error const & err) {
        LOGE(TAG, "{:04}: listener pool failure: {}", self_port, err.what());
    };

    listener_pool.on_accepted = [& peer_sockets, & reader_pool, self_port] (socket_t && sock) {
        auto sock_id = sock.id();
        LOGD(TAG, "{:04}: socket accepted: id={}: {}", self_port, sock_id, to_string(sock.saddr()));
        peer_sockets[sock.id()] = std::move(sock);
        reader_pool.add(sock_id);
    };

    listener_pool.add(listener_saddr, & err);

    if (err) {
        if (!s_is_error) {
            s_is_error = true;
            LOGE(TAG, "{:04}: listener pool failure: {}: {}", self_port, to_string(listener_saddr)
                , err.what());
        }

        return;
    }

    listener_pool.listen(MAX_NODES_COUNT * MAX_NODES_COUNT);

    s_listener_counter++;

    // Wait until all threads are initialized: all listeners are ready
    while (s_listener_counter != MAX_NODES_COUNT)
        ;

    if (s_is_error.load())
        return;

    connecting_pool.on_failure = [self_port] (netty::error const & err) {
        LOGE(TAG, "{:04}: {}", self_port, err.what());
    };

    connecting_pool.on_connected = [& connected_sockets, & writer_pool, host_id, self_port] (socket_t && sock) {
        auto sock_id = sock.id();
        LOGD(TAG, "{:04}: socket connected: id={}: {}", self_port, sock_id, to_string(sock.saddr()));
        connected_sockets[sock.id()] = std::move(sock);

        netty::p2p::hello_packet packet;
        packet.uuid = host_id;
        serializer_t::ostream_type out;
        serializer_t::pack(out, packet);
        writer_pool.enqueue(sock_id, out.data(), out.size());
    };

    connecting_pool.on_connection_refused = [self_port, & connecting_pool] (netty::socket4_addr saddr
            , netty::connection_failure_reason reason) {
        LOGE(TAG, "{:04}: connection refused for socket: {}: reason: {}, reconnecting"
            , self_port, to_string(saddr), to_string(reason));

        std::chrono::seconds timeout {1};
        LOGD(TAG, "{:04}: Reconnect after {}", self_port, timeout);
        connecting_pool.connect_timeout(timeout, saddr);
    };

    reader_pool.on_failure = [& connected_sockets, & peer_sockets, self_port] (socket_id id, netty::error const & err) {
        LOGE(TAG, "{:04}: read socket failure: id={}: {}", self_port, id, err.what());
        connected_sockets.erase(id);
        peer_sockets.erase(id);
    };

    reader_pool.on_data_ready = [& read_counter, self_port] (socket_id id, std::vector<char> && data) {
        LOGD(TAG, "{:04}: Input data ready: id={}: {} bytes", self_port, id, data.size());
        read_counter++;
    };

    reader_pool.locate_socket = [& peer_sockets] (socket_id id) {
        auto pos = peer_sockets.find(id);
        return pos == peer_sockets.end() ? nullptr : & pos->second;
    };

    writer_pool.on_failure = [& connected_sockets, & peer_sockets, self_port] (socket_id id, netty::error const & err) {
        LOGE(TAG, "{:04}: write socket failure: id={}: {}", self_port, id, err.what());
        connected_sockets.erase(id);
        peer_sockets.erase(id);
    };

    writer_pool.on_bytes_written = [& write_counter, self_port] (socket_id id, std::uint64_t n) {
        LOGD(TAG, "{:04}: bytes written: id={}: {}", self_port, id, n);
        write_counter++;
    };

    writer_pool.locate_socket = [& connected_sockets] (socket_id id) {
        auto pos = connected_sockets.find(id);
        return pos == connected_sockets.end() ? nullptr : & pos->second;
    };

    for (auto port = BASE_PORT; port < BASE_PORT + MAX_NODES_COUNT; port++) {
        if (port != self_port) {
            netty::socket4_addr server_addr {{127, 0, 0, 1}, port};
            connecting_pool.connect(server_addr);
        }

        listener_pool.step();
        connecting_pool.step();

        connecting_pool.apply_remove();
        listener_pool.apply_remove();
    }

    auto n = MAX_NODES_COUNT - 1;

    while (s_node_reverse_counter.load() > 0) {
        listener_pool.step();
        connecting_pool.step();
        writer_pool.step();
        reader_pool.step();

        if (read_counter == n && write_counter == n) {
            --s_node_reverse_counter;
            read_counter = 0;
        }

        connecting_pool.apply_remove();
        listener_pool.apply_remove();
        reader_pool.apply_remove();
        writer_pool.apply_remove();
    }
}

TEST_CASE("single channel connection") {
    netty::startup_guard startup_guard{};

    LOGD(TAG, "s_node_counter.is_lock_free = {}", s_node_counter.is_lock_free());

    std::vector<std::thread> thread_pool {MAX_NODES_COUNT};

    std::for_each(thread_pool.begin(), thread_pool.end(), [] (std::thread & thr) {
        thr = std::thread(worker);
    });

    std::for_each(thread_pool.begin(), thread_pool.end(), [] (std::thread & thr) {
        thr.join();
    });

    CHECK_EQ(s_node_counter.load(), MAX_NODES_COUNT);
    CHECK_EQ(s_node_reverse_counter.load(), 0);
}
