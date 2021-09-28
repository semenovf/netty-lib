////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.13 Initial version
////////////////////////////////////////////////////////////////////////////////
#include "pfs/timer_pool.hpp"
#include "pfs/net/p2p/envelope.hpp"
#include "pfs/net/p2p/hello.hpp"
#include "pfs/net/p2p/observer.hpp"
#include "pfs/net/p2p/qt5/discoverer.hpp"
#include "pfs/net/p2p/qt5/endpoint.hpp"
#include "pfs/net/p2p/qt5/listener.hpp"
#include <chrono>
#include <iostream>
#include <string>

#if QT5_CORE_ENABLED
#   include <QCoreApplication>
#   include <QTimer>
#else
#   error "Only implementation based on Qt5 available"
#endif

namespace p2p = pfs::net::p2p;
using inet4_addr = pfs::net::inet4_addr;
using discoverer_type = p2p::qt5::discoverer;
using listener_type = p2p::qt5::listener;
using observer_type = p2p::observer;
// using origin_endpoint = p2p::qt5::origin_endpoint;
// using peer_socket_type = p2p::peer_socket<p2p::backend_enum::qt5>;
// using connection_manager_type = p2p::connection_manager<p2p::backend_enum::qt5>;

static const std::string HELO_GREETING {"HELO"};
static std::chrono::milliseconds DEFAULT_EXPIRATION_TIMEOUT {3000};

int main (int argc, char * argv[])
{
    std::string program{argv[0]};

    QCoreApplication app(argc, argv);

    QTimer observer_timer;
    observer_timer.setTimerType(Qt::PreciseTimer);
    observer_timer.setSingleShot(true);

    discoverer_type discoverer;
    observer_type observer;
    listener_type listener;

    QTimer discovery_timer;
    discovery_timer.setTimerType(Qt::PreciseTimer);
    discovery_timer.setSingleShot(false);
    discovery_timer.setInterval(std::chrono::milliseconds{1000});

    QObject::connect(& discovery_timer, & QTimer::timeout, [& discoverer] {
        p2p::output_envelope envelope;
        envelope << p2p::hello {HELO_GREETING, 42042} << p2p::seal;
        discoverer.radiocast(envelope.data());
    });

    {
        discoverer_type::options options;
        options.listener_addr4 = "*";       // Bind to any address
        options.listener_port = 42222;
        options.listener_interface = "*";
        options.peer_addr4 = "227.1.1.255"; // Multicast radio

        if (!discoverer.set_options(std::move(options)))
            return EXIT_FAILURE;
    }

    {
        listener_type::options options;
        options.listener_addr4 = "*";       // Bind to any address
        options.listener_port = 42223;
        options.listener_interface = "*";

        if (!listener.set_options(std::move(options)))
            return EXIT_FAILURE;
    }

    listener.accepted.connect([] {
        std::cout
            << p2p::current_timepoint().count()
            << ": Listener: peer accepted" << std::endl;
    });

    listener.failure.connect([] (std::string const & s) {
        std::cout << "Listener Error: " << s << std::endl;
    });

    QObject::connect(& observer_timer, & QTimer::timeout, [& observer] {
        std::cout
            << p2p::current_timepoint().count()
            << ": Observer timer timed out: CHECK EXPIRATION" << std::endl;
        observer.check_expiration();
    });

    observer.rookie_accepted.connect([] (inet4_addr const & rookie, std::uint16_t port) {
        std::cout << p2p::current_timepoint().count()
            << ": Rookie accepted: "
            << std::to_string(rookie)
            << ":" << port << std::endl;

//         // FIXME {{{ Need connection manager (or channel manager)
//         connection_type conn;
//         conn.connected.connect([] (connection_type & /*this_conn*/) {
//             std::cout << p2p::current_timepoint().count() << ": Connected" << std::endl;
//         });
//
//         conn.disconnected.connect([] (connection_type & /*this_conn*/) {
//             std::cout << p2p::current_timepoint().count() << ": Disconnected" << std::endl;
//         });
//
//         conn.failure.connect([] (connection_type & /*this_conn*/, std::string const & error) {
//             std::cout << p2p::current_timepoint().count() << ": " << error << std::endl;
//         });
//
//         conn.connect(rookie, port);
//         // }}}
    });

    observer.nearest_expiration_changed.connect([& observer_timer] (std::chrono::milliseconds timepoint) {
        auto now = p2p::current_timepoint();

        if (now < timepoint) {
            std::cout << p2p::current_timepoint().count()
                << ": Observer timer: "
                << (observer_timer.isActive() ? "RESTART" : "START")
                << ": next: " << timepoint.count()
                << "; diff: " << (timepoint - now).count()
                << std::endl;

            observer_timer.start((timepoint - now).count());
        }
    });

    observer.expired.connect([] (inet4_addr const & addr, std::uint16_t port) {
        std::cout << p2p::current_timepoint().count()
            << ": Service expired: "
            << std::to_string(addr)
            << ":" << port << std::endl;
    });

    discoverer.incoming_data_received.connect([& observer] (inet4_addr const & sender
            , bool is_sender_local
            , std::string const & request) {

        p2p::hello hello{};
        p2p::input_envelope envelope {request};
        envelope >> hello >> p2p::unseal;

        if (envelope.success()) {
            assert(hello.greeting() == HELO_GREETING);

            if (!is_sender_local) {
                std::cout << p2p::current_timepoint().count()
                    << ": Listener: sender: "
                    << std::to_string(sender)
                    << " (" << (is_sender_local ? "local" : "remote") << ")"
                    << "; greeting: " << hello.greeting()
                    << "; port: " << hello.port()
                    << std::endl;
                observer.update(sender, hello.port(), DEFAULT_EXPIRATION_TIMEOUT);
            }
        } else {
            std::cerr << "Bad input hello" << std::endl;
        }
    });

    discoverer.failure.connect([] (std::string const & s) {
        std::cout << "Discovery Error: " << s << std::endl;
    });

    assert(discoverer.failure.size() == 1);

    if (!discoverer.start())
        return EXIT_FAILURE;

    discovery_timer.start();

    return app.exec();
}

