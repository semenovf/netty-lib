////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.13 Initial version
////////////////////////////////////////////////////////////////////////////////
#include "pfs/net/p2p/discoverer.hpp"
#include "pfs/net/p2p/envelope.hpp"
#include "pfs/net/p2p/hello.hpp"
#include "pfs/net/p2p/listener.hpp"
#include "pfs/net/p2p/observer.hpp"
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
using discoverer_type = p2p::discoverer<p2p::backend_enum::qt5>;

static const std::string HELO_GREETING {"HELO"};
static std::chrono::milliseconds DEFAULT_EXPIRATION_TIMEOUT {3000};

int main (int argc, char * argv[])
{
    std::string program{argv[0]};

    QCoreApplication app(argc, argv);
    QTimer discovery_timer;
    QTimer observer_timer;

    discovery_timer.setTimerType(Qt::PreciseTimer);
    discovery_timer.setSingleShot(false);
    discovery_timer.setInterval(std::chrono::milliseconds{1000});

    observer_timer.setTimerType(Qt::PreciseTimer);
    observer_timer.setSingleShot(true);

    discoverer_type discoverer;

    QObject::connect(& discovery_timer, & QTimer::timeout, [& discoverer] {
        p2p::output_envelope envelope;
        envelope << p2p::hello {HELO_GREETING, 42042} << p2p::seal;
        discoverer.radiocast(envelope.data());
    });

    discoverer_type::options options;
    options.listener_addr4 = "*"; // Bind to any address
    options.listener_port = 42222;
    options.listener_interface = "*";
    options.peer_addr4 = "227.1.1.255"; // Multicast radio

    if (!discoverer.set_options(std::move(options)))
        return EXIT_FAILURE;

    p2p::observer observer;

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
        std::cout << "Error: " << s << std::endl;
    });

    if (!discoverer.start())
        return EXIT_FAILURE;

    discovery_timer.start();

    return app.exec();
}

