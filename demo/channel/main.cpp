////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.07 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "Client.hpp"
#include "pfs/net/inet4_addr.hpp"
// #include "pfs/net/p2p/hello.hpp"
// #include "pfs/net/p2p/envelope.hpp"
// #include "pfs/net/p2p/observer.hpp"
// #include "pfs/net/p2p/qt5/discoverer.hpp"
// #include "pfs/net/p2p/qt5/endpoint.hpp"


// #include "pfs/net/p2p/qt5/timer_pool.hpp"
// #include <chrono>
// #include <string>
// #include <vector>

#if QT5_CORE_ENABLED
#   include <QCoreApplication>
#else
#   error "Implementation based on Qt5 available only"
#endif

#define TRACE_LEVEL 1

using uuid_t = pfs::uuid_t;
using inet4_addr_t = pfs::net::inet4_addr;

int main (int argc, char * argv[])
{
    QCoreApplication app(argc, argv);

    Client client1 {pfs::from_string<uuid_t>("01FH7H6YJB8XK9XNNZYR0WYDJ1")};
    Client client2 {pfs::from_string<uuid_t>("01FH7HB19B9T1CTKE5AXPTN74M")};

    if (!client1.start_listener(inet4_addr_t{127, 0, 0, 1}, 4242)) {
        fmt::print(stderr, "Starting listener failed for client: {}\n"
            , std::to_string(client1.uuid()));

        return EXIT_FAILURE;
    }

    fmt::print("Listener started for client: {}\n"
        , std::to_string(client1.uuid()));

    if (!client2.start_listener(inet4_addr_t{127, 0, 0, 1}, 4243)) {
        fmt::print(stderr, "Starting listener failed for client: {}\n"
            , std::to_string(client2.uuid()));

        return EXIT_FAILURE;
    }

    fmt::print("Listener started for client: {}\n"
        , std::to_string(client2.uuid()));

//     if (listener.started()) {
// #if TRACE_LEVEL >= 1
//         fmt::print("{}: Listener: started: {}:{}\n"
//             , p2p::current_timepoint().count()
//             , std::to_string(listener.address())
//             , listener.port());
// #endif
//     }
//
//     timer_pool.start_discovery_timer(std::chrono::milliseconds{1000});
//     timer_pool.start_pending_timer(std::chrono::milliseconds{30000});

    return app.exec();
}
