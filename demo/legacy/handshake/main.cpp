////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/uuid.hpp"
#include "pfs/fmt.hpp"
#include "pfs/net/inet4_addr.hpp"
#include "pfs/net/p2p/framework.hpp"
#include "pfs/net/p2p/observer.hpp"
#include "pfs/net/p2p/qt5/discoverer.hpp"
#include "pfs/net/p2p/qt5/udp_reader.hpp"
#include "pfs/net/p2p/qt5/udp_writer.hpp"
#include "pfs/net/p2p/qt5/timer_pool.hpp"
#include <chrono>
#include <string>

#if QT5_CORE_ENABLED
#   include <QCoreApplication>
#else
#   error "Implementation based on Qt5 available only"
#endif

static constexpr std::size_t PACKET_SIZE = 256;
static const std::chrono::milliseconds DEFAULT_DISCOVERY_INTERVAL {1000};
static const std::chrono::milliseconds DEFAULT_EXPIRATION_TIMEOUT {3000};
static const std::uint16_t LISTENER_PORT = 42223;
static const pfs::uuid_t UUID = pfs::generate_uuid();

namespace p2p = pfs::net::p2p;
using inet4_addr = pfs::net::inet4_addr;

using timer_pool_type      = p2p::qt5::timer_pool;
using discoverer_type      = p2p::qt5::discoverer;
using observer_type        = p2p::observer;
using reader_type          = p2p::qt5::udp_reader<PACKET_SIZE>;
using writer_type          = p2p::qt5::udp_writer<PACKET_SIZE>;
using framework_type       = p2p::framework<
                                  timer_pool_type
                                , discoverer_type
                                , observer_type
                                , reader_type
                                , writer_type>;

int main (int argc, char * argv[])
{
    std::string program{argv[0]};

    fmt::print("My name is {}\n", std::to_string(UUID));

    QCoreApplication app(argc, argv);

    framework_type framework {UUID};

    // Configure discoverer and listener
    discoverer_type::options discoverer_opts;
    discoverer_opts.listener_addr4 = inet4_addr{}; // Bind to any address
    discoverer_opts.listener_port = 42222;
    discoverer_opts.listener_interface = "*";
    discoverer_opts.peer_addr4 = inet4_addr{227, 1, 1, 255}; // Multicast radio
    discoverer_opts.interval = DEFAULT_DISCOVERY_INTERVAL;
    discoverer_opts.expiration_timeout = DEFAULT_EXPIRATION_TIMEOUT;

    reader_type::options reader_opts;
    reader_opts.listener_addr4 = inet4_addr{}; // Bind to any address
    reader_opts.listener_port = LISTENER_PORT;
    reader_opts.listener_interface = "*";

    if (!framework.configure(std::move(discoverer_opts), std::move(reader_opts)))
        return EXIT_FAILURE;

    framework.failure.connect([] (std::string const & error) {
        fmt::print(stderr, "!! Error: {}\n", error);
    });

    if (!framework.start())
        return EXIT_FAILURE;

    return app.exec();
}
