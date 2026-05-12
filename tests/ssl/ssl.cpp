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
#include "pfs/netty/simple_input_controller.hpp"
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
using input_controller_t = netty::simple_input_controller<socket_t::socket_id, serializer_traits_t>;

constexpr std::uint16_t PORT = 4242;
constexpr char const * TEST_DATA = "Hello, Secured Sockets!";

static std::vector<netty::interruptable *> s_interruptables;

static void sigterm_handler (int sig)
{
    INFO("Force signal: ", sig);

    for (auto i: s_interruptables)
        i->interrupt();
}

inline void quit ()
{
    sigterm_handler(SIGTERM);
}

struct server: public netty::interruptable
{
    listener_handshake_pool_t handshake_pool;
    listener_pool_t listener_pool;
    reader_pool_t reader_pool;
    writer_pool_t writer_pool;
    socket_pool_t socket_pool;
    input_controller_t input_controller;

    server ()
        : netty::interruptable()
        , listener_pool(handshake_pool)
    {
        netty::listener_options opts;
        opts.saddr = netty::socket4_addr{netty::inet4_addr::any_addr_value, PORT};
        opts.tls.cert_file = std::string("./cert.pem");
        opts.tls.key_file = std::string("./key.pem");

        listener_pool.on_failure = [] (netty::error const & err) {
            FAIL_CHECK("Listener pool failure: ", std::string(err.what()));
            quit();
        };

        listener_pool.add(opts);

        listener_pool.on_accepted = [this] (socket_t && sock) {
            MESSAGE("Socket accepted: ", sock.id());

            auto saddr = sock.saddr();
            reader_pool.add(sock.id());
            writer_pool.add(sock.id());
            socket_pool.add_accepted(std::move(sock));
        };

        reader_pool.on_failure = [this] (socket_t::socket_id, netty::error const & err)
        {
            FAIL_CHECK("Read from socket failure: ", err.what());
            quit();
        };

        reader_pool.on_disconnected = [this] (socket_t::socket_id sid)
        {
            MESSAGE("Reader socket disconnected: ", sid);
            quit();
        };

        reader_pool.on_data_ready = [this] (socket_t::socket_id sid, archive_t ar)
        {
            input_controller.process_input(sid, std::move(ar));
        };

        reader_pool.locate_socket = [this] (socket_t::socket_id sid)
        {
            return socket_pool.locate(sid);
        };

        writer_pool.on_failure = [this] (socket_t::socket_id, netty::error const & err)
        {
            FAIL_CHECK("Write to socket failure: {}", err.what());
            quit();
        };

        writer_pool.on_disconnected = [this] (socket_t::socket_id sid)
        {
            FAIL_CHECK("Writer socket disconnected: ", sid);
            quit();
        };

        writer_pool.locate_socket = [this] (socket_t::socket_id sid)
        {
            return socket_pool.locate(sid);
        };

        input_controller.on_data_ready = [this] (socket_t::socket_id sid, archive_t ar) {
            std::string text = std::string(ar.data(), ar.data() + ar.size());
            MESSAGE("Data ready on server: ", ar.size(), " bytes" );
            CHECK(text == std::string(TEST_DATA));

            writer_pool.enqueue(sid, text.c_str(), text.size());
        };
    }

    unsigned int step_unsafe ()
    {
        unsigned int result = 0;

        result += listener_pool.step();
        result += reader_pool.step();
        result += writer_pool.step();

        // Remove trash
        listener_pool.apply_remove();
        reader_pool.apply_remove();
        writer_pool.apply_remove();
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
    writer_pool_t     writer_pool;
    input_controller_t input_controller;
    int transfer_counter = 10;

    client ()
        : netty::interruptable()
        , handshake_pool()
        , conn_pool(handshake_pool)
    {
        conn_pool.on_failure = [] (socket_t::socket_id, netty::error const & err) {
            FAIL_CHECK("Connecting pool failure: ", std::string(err.what()));
            quit();
        };

        conn_pool.on_connected = [this] (socket_t && sock) {
            auto sid = sock.id();
            MESSAGE("Socket connected: ", sid);
            auto saddr = sock.saddr();
            reader_pool.add(sid);
            socket_pool.add_connected(std::move(sock));

            std::string text(TEST_DATA);
            writer_pool.enqueue(sid, text.c_str(), text.size());
        };

        conn_pool.on_connection_refused = [] (netty::socket4_addr, netty::connection_failure_reason reason) {
            FAIL_CHECK("Connection refused: ", to_string(reason));
            quit();
        };

        reader_pool.on_failure = [] (socket_t::socket_id, netty::error const & err)
        {
            FAIL_CHECK("Reader pool failure: ", std::string(err.what()));
            quit();
        };

        reader_pool.on_data_ready = [this] (socket_t::socket_id sid, archive_t ar)
        {
            input_controller.process_input(sid, std::move(ar));
        };

        reader_pool.on_disconnected = [] (socket_t::socket_id)
        {
            MESSAGE("Disconnected");
        };

        reader_pool.locate_socket = [this] (socket_t::socket_id sid)
        {
            return socket_pool.locate(sid);
        };

        writer_pool.on_failure = [this] (socket_t::socket_id, netty::error const & err)
        {
            FAIL_CHECK("Write to socket failure: ", err.what());
            quit();
        };

        writer_pool.on_disconnected = [this] (socket_t::socket_id)
        {
            MESSAGE("Socket disconnected");
        };

        writer_pool.locate_socket = [this] (socket_t::socket_id sid)
        {
            return socket_pool.locate(sid);
        };

        input_controller.on_data_ready = [this] (socket_t::socket_id sid, archive_t ar) {
            std::string text = std::string(ar.data(), ar.data() + ar.size());
            MESSAGE("Data ready on server: ", ar.size(), " bytes" );
            CHECK(text == std::string(TEST_DATA));

            --transfer_counter;

            if (transfer_counter > 0)
                writer_pool.enqueue(sid, text.c_str(), text.size());
            else
                quit();
        };
    }

    bool connect (netty::socket4_addr remote_saddr)
    {
        netty::connection_options opts;
        opts.remote_saddr =remote_saddr;
        opts.tls.cert_file = std::string("./cert.pem");
        opts.tls.key_file  = std::string("./key.pem");

        auto rs = conn_pool.connect(opts);

        if (rs == netty::conn_status::failure)
            return false;

        return true;
    }

    unsigned int step_unsafe ()
    {
        unsigned int result = 0;

        result += conn_pool.step();
        result += reader_pool.step();
        result += writer_pool.step();

        // Remove trash
        conn_pool.apply_remove();
        reader_pool.apply_remove();
        writer_pool.apply_remove();
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

    server srv;
    s_interruptables.push_back(& srv);
    srv.listener_pool.listen();

    std::thread client_thread([] () {
        client c;
        s_interruptables.push_back(& c);
        REQUIRE(c.connect(netty::socket4_addr{netty::inet4_addr::localhost_addr_value, PORT}));
        c.run();
    });

    srv.run();
    client_thread.join();
}

