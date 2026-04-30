////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.04.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"
#include "../serializer_traits.hpp"
#include "../tools.hpp"
#include "pfs/netty/connecting_pool.hpp"
#include "pfs/netty/interruptable.hpp"
#include "pfs/netty/listener_pool.hpp"
#include "pfs/netty/poller_types.hpp"
#include "pfs/netty/reader_pool.hpp"
#include "pfs/netty/simple_frame.hpp"
#include "pfs/netty/socket_pool.hpp"
#include "pfs/netty/startup.hpp"
#include "pfs/netty/writer_queue.hpp"
#include "pfs/netty/writer_pool.hpp"
#include "pfs/netty/ssl/client_handshake_pool.hpp"
#include "pfs/netty/ssl/listener_handshake_pool.hpp"
#include "pfs/netty/ssl/tls_listener.hpp"
#include "pfs/netty/ssl/tls_socket.hpp"
#include <pfs/countdown_timer.hpp>
#include <pfs/signal_guard.hpp>
#include <iostream>

using socket_t = netty::ssl::tls_socket;
using listener_t = netty::ssl::tls_listener;
using frame_t = netty::simple_frame<serializer_traits_t>;
using writer_queue_t = netty::writer_queue<serializer_traits_t, frame_t>;
using client_handshake_pool_t = netty::ssl::client_handshake_pool;
using listener_handshake_pool_t = netty::ssl::listener_handshake_pool;
using connecting_pool_t = netty::connecting_pool<socket_t, netty::connecting_poll_poller_t, client_handshake_pool_t>;
using listener_pool_t = netty::listener_pool<listener_t, socket_t, netty::listener_poll_poller_t, listener_handshake_pool_t>;
using reader_pool_t = netty::reader_pool<socket_t, netty::reader_poll_poller_t, archive_t>;
using writer_pool_t = netty::writer_pool<socket_t, netty::writer_poll_poller_t, writer_queue_t>;
using socket_pool_t = netty::socket_pool<socket_t>;

constexpr std::uint16_t PORT = 4242;

static std::vector<netty::interruptable *> s_interruptables;

static void sigterm_handler (int sig)
{
    std::cerr << "Force signal: " << sig << std::endl;

    for (auto i: s_interruptables)
        i->interrupt();
}

struct server: public netty::interruptable
{
    listener_handshake_pool_t handshake_pool;
    listener_pool_t listener_pool;
    writer_pool_t writer_pool;
    socket_pool_t socket_pool;

    server ()
        : netty::interruptable()
        , listener_pool(handshake_pool)
    {
        netty::ssl::tls_options opts;
        opts.cert_file = std::string("./cert.pem");
        opts.key_file = std::string("./key.pem");

        listener_t listener {netty::socket4_addr{netty::inet4_addr::any_addr_value, PORT}, std::move(opts)};

        listener_pool.on_failure = [] (netty::error const & err) {
            std::cerr << "listener pool failure: " << err.what() << std::endl;
        };

        listener_pool.on_accepted = [this] (socket_t && sock) {
            MESSAGE("Socket accepted: ", sock.id());

            auto saddr = sock.saddr();
            writer_pool.add(sock.id());
            socket_pool.add_accepted(std::move(sock));
        };

        listener_pool.add(std::move(listener));
    }

    unsigned int step_unsafe ()
    {
        unsigned int result = 0;

        result += listener_pool.step();
        // result += writer_pool.step(); // FIXME

        // Remove trash
        listener_pool.apply_remove();
        // writer_pool.apply_remove(); // FIXME
        socket_pool.apply_remove(); // Must be last in the removing sequence

        return result;
    }

    void run (std::chrono::milliseconds loop_interval = std::chrono::milliseconds{10})
    {
        clear_interrupted();

        while (!interrupted()) {
            pfs::countdown_timer<std::milli> countdown_timer {loop_interval};
            auto n = step_unsafe();

            if (n == 0)
                std::this_thread::sleep_for(countdown_timer.remain());
        }
    }
};

struct client: public netty::interruptable
{
    client_handshake_pool_t handshake_pool;
    connecting_pool_t conn_pool;
    reader_pool_t     reader_pool;
    socket_pool_t     socket_pool;

    client ()
        : netty::interruptable()
        , handshake_pool()
        , conn_pool(handshake_pool)
    {
        conn_pool.on_failure = [] (socket_t::socket_id, netty::error const & err) {
            std::cerr << "connecting pool failure: " << err.what() << std::endl;
        };

        conn_pool.on_connected = [this] (socket_t && sock) {
            MESSAGE("Socket connected: ", sock.id());

            auto saddr = sock.saddr();
            reader_pool.add(sock.id());
            socket_pool.add_connected(std::move(sock));

            // FIXME REMOVE
            sigterm_handler(SIGTERM);
        };

        conn_pool.on_connection_refused = [] (netty::socket4_addr, netty::connection_failure_reason reason) {
            std::cerr << "connection refused: reason=" << to_string(reason) << std::endl;
        };
    }

    bool connect (netty::socket4_addr remote_saddr)
    {
        netty::ssl::tls_options opts;
        opts.cert_file = std::string("./cert.pem");
        opts.key_file = std::string("./key.pem");

        auto rs = conn_pool.connect(remote_saddr, std::move(opts));

        if (rs == netty::conn_status::failure)
            return false;

        return true;
    }

    unsigned int step_unsafe ()
    {
        unsigned int result = 0;

        result += conn_pool.step();
        // result += reader_pool.step(); // FIXME

        // Remove trash
        conn_pool.apply_remove();
        // reader_pool.apply_remove(); // FIXME
        socket_pool.apply_remove(); // Must be last in the removing sequence

        return result;
    }

    void run (std::chrono::milliseconds loop_interval = std::chrono::milliseconds{10})
    {
        clear_interrupted();

        while (!interrupted()) {
            pfs::countdown_timer<std::milli> countdown_timer {loop_interval};
            auto n = step_unsafe();

            if (n == 0)
                std::this_thread::sleep_for(countdown_timer.remain());
        }
    }
};

TEST_CASE("basic") {
    netty::startup_guard netty_startup;
    pfs::signal_guard sigint_guard(SIGINT, sigterm_handler);
    pfs::signal_guard sigterm_guard(SIGTERM, sigterm_handler);

#if 0
    server srv;

    srv.listener_pool.listen(10);
    s_interruptables.push_back(& srv);

    srv.run();
#else
    client c;
    c.connect(netty::socket4_addr{netty::inet4_addr::localhost_addr_value, PORT});
    s_interruptables.push_back(& c);
    c.run();
#endif


}

