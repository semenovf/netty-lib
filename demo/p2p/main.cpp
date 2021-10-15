////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.13 Initial version
////////////////////////////////////////////////////////////////////////////////
#include "pfs/net/p2p/hello.hpp"
#include "pfs/net/p2p/envelope.hpp"
#include "pfs/net/p2p/observer.hpp"
#include "pfs/net/p2p/qt5/discoverer.hpp"
#include "pfs/net/p2p/qt5/endpoint.hpp"
#include "pfs/net/p2p/qt5/listener.hpp"
#include "pfs/net/p2p/qt5/speaker.hpp"
#include "pfs/net/p2p/qt5/timer_pool.hpp"
#include <chrono>
#include <string>
#include <vector>

#if QT5_CORE_ENABLED
#   include <QCoreApplication>
#else
#   error "Implementation based on Qt5 available only"
#endif

#define TRACE_LEVEL 1

namespace p2p = pfs::net::p2p;
using inet4_addr = pfs::net::inet4_addr;
using discoverer_type = p2p::qt5::discoverer;
using listener_type = p2p::qt5::listener;
using observer_type = p2p::observer;
using speaker_type = p2p::qt5::speaker;
using shared_endpoint = p2p::qt5::shared_endpoint;
using timer_pool_type = p2p::qt5::timer_pool;

static const std::string HELO_GREETING {"HELO"};
static const std::chrono::milliseconds DEFAULT_EXPIRATION_TIMEOUT {3000};
static const std::uint16_t LISTENER_PORT = 42223;
static const pfs::uuid_t UUID = pfs::generate_uuid();

int main (int argc, char * argv[])
{
    std::string program{argv[0]};

    fmt::print("My name is {}\n", std::to_string(UUID));

    QCoreApplication app(argc, argv);

    timer_pool_type timer_pool;
    discoverer_type discoverer;
    observer_type observer;
    listener_type listener;
    speaker_type speaker;

    timer_pool.discovery_timer_timeout.connect([& discoverer] () {
        p2p::output_envelope<> envelope;
        envelope << p2p::hello {HELO_GREETING, UUID, LISTENER_PORT} << p2p::seal;
        discoverer.radiocast(envelope.data());
    });

    timer_pool.observer_timer_timeout.connect([& observer] () {
#if TRACE_LEVEL >= 2
        fmt::print("{}: Observer: timer timed out: checking expiration\n"
            , p2p::current_timepoint().count());
#endif
        observer.check_expiration();
    });

    timer_pool.pending_timer_timeout.connect([& listener, & speaker] () {
        listener.check_expiration();
        speaker.check_expiration();
    });

    {
        discoverer_type::options options;
        options.listener_addr4 = inet4_addr{}; // Bind to any address
        options.listener_port = 42222;
        options.listener_interface = "*";
        options.peer_addr4 = inet4_addr{227, 1, 1, 255}; // Multicast radio

        if (!discoverer.set_options(std::move(options)))
            return EXIT_FAILURE;
    }

    {
        listener_type::options options;
        options.listener_addr4 = inet4_addr{}; // Bind to any address
        options.listener_port = LISTENER_PORT;
        options.listener_interface = "*";

        if (!listener.set_options(std::move(options)))
            return EXIT_FAILURE;
    }

    listener.accepted.connect([] (shared_endpoint ep) {
#if TRACE_LEVEL >= 1
        fmt::print("{}: Listener: peer accepted: {}:{}\n"
            , p2p::current_timepoint().count()
            , std::to_string(ep->peer_address())
            , ep->peer_port());
#endif
    });

    listener.failure.connect([] (std::string const & s) {
        fmt::print("{}: Listener: Error: {}\n"
            , p2p::current_timepoint().count()
            , s);
    });

    speaker.connected.connect([] (shared_endpoint ep) {
#if TRACE_LEVEL >= 1
        fmt::print("{}: Speaker: socket connected: {} ({}:{})\n"
            , p2p::current_timepoint().count()
            , std::to_string(ep->uuid())
            , std::to_string(ep->peer_address())
            , ep->peer_port());
#endif
    });

    speaker.disconnected.connect([] (shared_endpoint ep) {
#if TRACE_LEVEL >= 1
        fmt::print("{}: Speaker: socket disconnected: {} ({}:{})\n"
            , p2p::current_timepoint().count()
            , std::to_string(ep->uuid())
            , std::to_string(ep->peer_address())
            , ep->peer_port());
#endif
    });

    speaker.endpoint_failure.connect([] (shared_endpoint ep, std::string const & error) {
#if TRACE_LEVEL >= 1
        fmt::print(stderr, "{}: Speaker: Error: {} ({}:{}): {}\n"
            , p2p::current_timepoint().count()
            , std::to_string(ep->uuid())
            , std::to_string(ep->peer_address())
            , ep->peer_port()
            , error);
#endif
    });

    observer.rookie_accepted.connect([& speaker] (pfs::uuid_t peer_uuid
        , inet4_addr const & rookie
        , std::uint16_t port) {
#if TRACE_LEVEL >= 1
        fmt::print("{}: Observer: rookie accepted: Hello, {} ({}:{})\n"
            , p2p::current_timepoint().count()
            , std::to_string(peer_uuid)
            , std::to_string(rookie)
            , port);
#endif

        speaker.connect(peer_uuid, rookie, port);
    });

    observer.nearest_expiration_changed.connect([& timer_pool/*observer_timer*/] (
            std::chrono::milliseconds timepoint) {
        auto now = p2p::current_timepoint();

        if (now < timepoint) {
#if TRACE_LEVEL >= 3
            fmt::print("{}: Observer: next: {}; diff: {}\n"
                , p2p::current_timepoint().count()
                , timepoint.count()
                , (timepoint - now).count());
#endif

            timer_pool.start_observer_timer(timepoint - now);
        }
    });

    observer.expired.connect([] (pfs::uuid_t uuid, inet4_addr const & addr, std::uint16_t port) {
#if TRACE_LEVEL >= 1
        fmt::print("{}: Observer: service expired: Goodbye, {} ({}:{})\n"
            , p2p::current_timepoint().count()
            , std::to_string(uuid)
            , std::to_string(addr)
            , port);
#endif
    });

    discoverer.incoming_data_received.connect([& observer] (inet4_addr const & sender
            , bool is_sender_local
            , std::string const & request) {

        p2p::hello hello{};
        p2p::input_envelope<> envelope {request};
        envelope >> hello >> p2p::unseal;

        if (envelope.success()) {
            assert(hello.greeting() == HELO_GREETING);

            if (!is_sender_local) {
#if TRACE_LEVEL >= 3
                fmt::print("{}: Listener: sender: {} ({}:{}) ({}); greeting: {}\n"
                    , p2p::current_timepoint().count()
                    , std::to_string(hello.uuid())
                    , std::to_string(sender)
                    , hello.port()
                    , (is_sender_local ? "local" : "remote")
                    , hello.greeting());
#endif
                observer.update(hello.uuid()
                    , sender
                    , hello.port()
                    , DEFAULT_EXPIRATION_TIMEOUT);
            }
        } else {
            fmt::print(stderr, "Bad input hello\n");
        }
    });

    discoverer.failure.connect([] (std::string const & error) {
        fmt::print(stderr, "{}: Discoverer: Error: {}\n"
            , p2p::current_timepoint().count()
            , error);
    });

    assert(discoverer.failure.size() == 1);

    if (!listener.start())
        return EXIT_FAILURE;

    if (!discoverer.start())
        return EXIT_FAILURE;

    if (listener.started()) {
#if TRACE_LEVEL >= 1
        fmt::print("{}: Listener: started: {}:{}\n"
            , p2p::current_timepoint().count()
            , std::to_string(listener.address())
            , listener.port());
#endif
    }

    timer_pool.start_discovery_timer(std::chrono::milliseconds{1000});
    timer_pool.start_pending_timer(std::chrono::milliseconds{30000});

    return app.exec();
}
