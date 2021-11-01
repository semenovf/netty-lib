////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.13 Initial version
//      2021.11.01 New version using UDT.
////////////////////////////////////////////////////////////////////////////////
#define PFS_NET__P2P_TRACE_LEVEL 3
#include "pfs/net/p2p/trace.hpp"
#include "pfs/net/inet4_addr.hpp"
#include "pfs/net/p2p/algorithm.hpp"
#include "pfs/net/p2p/qt5/udp_socket.hpp"
#include "pfs/net/p2p/udt/poller.hpp"
#include "pfs/net/p2p/udt/udp_socket.hpp"

namespace p2p {
using inet4_addr           = pfs::net::inet4_addr;
using discovery_udp_socket = pfs::net::p2p::qt5::udp_socket;
using reliable_udp_socket  = pfs::net::p2p::udt::udp_socket;
using poller               = pfs::net::p2p::udt::poller;
static constexpr std::size_t DEFAULT_PACKET_SIZE = 64;

using algorithm = pfs::net::p2p::algorithm<
      discovery_udp_socket
    , reliable_udp_socket
    , poller
    , DEFAULT_PACKET_SIZE>;
} // namespace p2p

namespace {
    const pfs::uuid_t UUID = pfs::generate_uuid();

    std::chrono::milliseconds const DISCOVERY_TRANSMIT_INTERVAL {100};
    std::chrono::milliseconds const PEER_EXPIRATION_TIMEOUT {500};
    std::chrono::milliseconds const POLL_INTERVAL {10};
    //std::chrono::milliseconds const POLL_INTERVAL {1000};

    const p2p::inet4_addr TARGET_ADDR{227, 1, 1, 255};
    const p2p::inet4_addr DISCOVERY_ADDR{TARGET_ADDR};
    const std::uint16_t DISCOVERY_PORT{4242u};
}

struct configurator
{
    p2p::inet4_addr discovery_address () const noexcept { return DISCOVERY_ADDR; }
    std::uint16_t   discovery_port () const noexcept { return DISCOVERY_PORT; }
    std::chrono::milliseconds discovery_transmit_interval () const noexcept { return DISCOVERY_TRANSMIT_INTERVAL; }
    std::chrono::milliseconds expiration_timeout () const noexcept { return PEER_EXPIRATION_TIMEOUT; }
    std::chrono::milliseconds poll_interval () const noexcept { return POLL_INTERVAL; }
    p2p::inet4_addr listener_address () const noexcept { return p2p::inet4_addr{127, 0, 0, 1}; }
    std::uint16_t   listener_port () const noexcept { return 4224u; }
    int backlog () const noexcept { return 10; }
};

void on_failure (std::string const & error)
{
    fmt::print(stderr, "!ERROR: {}\n", error);
}

void on_rookie_accepted (pfs::uuid_t uuid
    , pfs::net::inet4_addr const & addr
    , std::uint16_t port)
{
    TRACE_1("HELO: {} ({}:{})"
        , std::to_string(uuid)
        , std::to_string(addr)
        , port);
}

void on_writer_ready (pfs::uuid_t uuid
    , pfs::net::inet4_addr const & addr
    , std::uint16_t port)
{
    TRACE_1("WRITER READY: {} ({}:{})"
        , std::to_string(uuid)
        , std::to_string(addr)
        , port);
}

void on_peer_expired (pfs::uuid_t uuid
    , pfs::net::inet4_addr const & addr
    , std::uint16_t port)
{
    TRACE_1("EXPIRED: {} ({}:{})"
        , std::to_string(uuid)
        , std::to_string(addr)
        , port);
}

void worker (p2p::algorithm & peer)
{
    TRACE_1("Peer started: {}", std::to_string(peer.uuid()));

    while (true) {
        peer.loop();
    }

    TRACE_1("Peer finished: {}", std::to_string(peer.uuid()));
}

int main (int argc, char * argv[])
{
    std::string program{argv[0]};

    fmt::print("My name is {}\n", std::to_string(UUID));

    p2p::algorithm peer {UUID};
    peer.failure.connect(on_failure);
    peer.rookie_accepted.connect(on_rookie_accepted);
    peer.writer_ready.connect(on_writer_ready);
    peer.peer_expired.connect(on_peer_expired);

    assert(peer.configure(configurator{}));

    // FIXME unable process multicast group
    //REQUIRE(peer1.add_discovery_target(p2p::inet4_addr_t{224, 0, 0, 1}, 4243));
    peer.add_discovery_target(TARGET_ADDR, DISCOVERY_PORT);

    worker(peer);

    return EXIT_SUCCESS;
}
