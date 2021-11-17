////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.13 Initial version
//      2021.11.01 New version using UDT.
////////////////////////////////////////////////////////////////////////////////
#define PFS_NET_P2P__TRACE_LEVEL 3
#include "pfs/net/p2p/trace.hpp"
#include "pfs/net/inet4_addr.hpp"
#include "pfs/net/p2p/algorithm.hpp"
#include "pfs/net/p2p/qt5/udp_socket.hpp"
#include "pfs/net/p2p/udt/api.hpp"

static char loremipsum[] =
"1.Lorem ipsum dolor sit amet, consectetuer adipiscing elit,    \n\
2.sed diam nonummy nibh euismod tincidunt ut laoreet dolore     \n\
3.magna aliquam erat volutpat. Ut wisi enim ad minim veniam,    \n\
4.quis nostrud exerci tation ullamcorper suscipit lobortis      \n\
5.nisl ut aliquip ex ea commodo consequat. Duis autem vel eum   \n\
6.iriure dolor in hendrerit in vulputate velit esse molestie    \n\
7.consequat, vel illum dolore eu feugiat nulla facilisis at     \n\
8.vero eros et accumsan et iusto odio dignissim qui blandit     \n\
9.praesent luptatum zzril delenit augue duis dolore te feugait  \n\
10.nulla facilisi. Nam liber tempor cum soluta nobis eleifend    \n\
11.option congue nihil imperdiet doming id quod mazim placerat   \n\
12.facer possim assum. Typi non habent claritatem insitam; est   \n\
13.usus legentis in iis qui facit eorum claritatem.              \n\
14.Investigationes demonstraverunt lectores legere me lius quod  \n\
15.ii legunt saepius. Claritas est etiam processus dynamicus,    \n\
16.qui sequitur mutationem consuetudium lectorum. Mirum est      \n\
17.notare quam littera gothica, quam nunc putamus parum claram,  \n\
18.anteposuerit litterarum formas humanitatis per seacula quarta \n\
19.decima et quinta decima. Eodem modo typi, qui nunc nobis      \n\
20.videntur parum clari, fiant sollemnes in futurum.             \n\
21.Lorem ipsum dolor sit amet, consectetuer adipiscing elit,     \n\
22.sed diam nonummy nibh euismod tincidunt ut laoreet dolore     \n\
23.magna aliquam erat volutpat. \"Ut wisi enim ad minim veniam,  \n\
24.quis nostrud exerci tation ullamcorper suscipit lobortis      \n\
25.nisl ut aliquip ex ea commodo consequat. Duis autem vel eum   \n\
26.iriure dolor in hendrerit in vulputate velit esse molestie    \n\
27.consequat, vel illum dolore eu feugiat nulla facilisis at     \n\
28.vero eros et accumsan et iusto odio dignissim qui blandit     \n\
29.praesent luptatum zzril delenit augue duis dolore te feugait  \n\
30.nulla facilisi. Nam liber tempor cum soluta nobis eleifend    \n\
31.option congue nihil imperdiet doming id quod mazim placerat   \n\
32.facer possim assum. Typi non habent claritatem insitam; est   \n\
33.usus legentis in iis qui facit eorum claritatem.              \n\
34.Investigationes demonstraverunt lectores legere me lius quod  \n\
35.ii legunt saepius. Claritas est etiam processus dynamicus,    \n\
36.qui sequitur mutationem consuetudium lectorum. Mirum est      \n\
37.notare quam littera gothica, quam nunc putamus parum claram,  \n\
38.anteposuerit litterarum formas humanitatis per seacula quarta \n\
39.decima et quinta decima.\" Eodem modo typi, qui nunc nobis    \n\
40.videntur parum clari, fiant sollemnes in futurum.";

namespace p2p {
using inet4_addr           = pfs::net::inet4_addr;
using discovery_udp_socket = pfs::net::p2p::qt5::udp_socket;
using reliable_socket_api  = pfs::net::p2p::udt::api;
using poller               = pfs::net::p2p::udt::poller;
static constexpr std::size_t PACKET_SIZE = 64;

using algorithm = pfs::net::p2p::algorithm<
      discovery_udp_socket
    , reliable_socket_api
    , PACKET_SIZE>;

using packet_type = algorithm::packet_type;
} // namespace p2p

namespace {
    const pfs::uuid_t UUID = pfs::generate_uuid();

    std::chrono::milliseconds const DISCOVERY_TRANSMIT_INTERVAL {100};
    std::chrono::milliseconds const PEER_EXPIRATION_TIMEOUT {2000};
    std::chrono::milliseconds const POLL_INTERVAL {10};
    //std::chrono::milliseconds const POLL_INTERVAL {1000};

    const p2p::inet4_addr TARGET_ADDR{227, 1, 1, 255};
    const p2p::inet4_addr DISCOVERY_ADDR{};
    const std::uint16_t DISCOVERY_PORT{4242u};
}

struct configurator
{
    p2p::inet4_addr discovery_address () const noexcept { return DISCOVERY_ADDR; }
    std::uint16_t   discovery_port () const noexcept { return DISCOVERY_PORT; }
    std::chrono::milliseconds discovery_transmit_interval () const noexcept { return DISCOVERY_TRANSMIT_INTERVAL; }
    std::chrono::milliseconds expiration_timeout () const noexcept { return PEER_EXPIRATION_TIMEOUT; }
    std::chrono::milliseconds poll_interval () const noexcept { return POLL_INTERVAL; }
    p2p::inet4_addr listener_address () const noexcept { return p2p::inet4_addr{}; }
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

// void on_writer_ready (pfs::uuid_t uuid
//     , pfs::net::inet4_addr const & addr
//     , std::uint16_t port)
// {
//     TRACE_1("WRITER READY: {} ({}:{})"
//         , std::to_string(uuid)
//         , std::to_string(addr)
//         , port);
//
//     peer.enqueue()
// }

void on_peer_expired (pfs::uuid_t uuid
    , pfs::net::inet4_addr const & addr
    , std::uint16_t port)
{
    TRACE_1("EXPIRED: {} ({}:{})"
        , std::to_string(uuid)
        , std::to_string(addr)
        , port);
}

void on_packet_received (pfs::net::inet4_addr const & addr
    , std::uint16_t port
    , p2p::packet_type const & pkt)
{
    TRACE_1("Packet received from: {} ({}:{})"
        , std::to_string(pkt.uuid)
        , std::to_string(addr)
        , port);
}

void worker (p2p::algorithm & peer)
{
    while (true) {
        peer.loop();
    }
}

int main (int argc, char * argv[])
{
    std::string program{argv[0]};

    fmt::print("My name is {}\n", std::to_string(UUID));

    if (!p2p::algorithm::startup())
        return EXIT_FAILURE;

    p2p::algorithm peer {UUID};
    peer.failure.connect(on_failure);
    peer.rookie_accepted.connect(on_rookie_accepted);

    peer.writer_ready.connect([& peer] (pfs::uuid_t uuid
            , pfs::net::inet4_addr const & addr
            , std::uint16_t port) {
        TRACE_1("WRITER READY: {} ({}:{})"
            , std::to_string(uuid)
            , std::to_string(addr)
            , port);

        peer.enqueue(uuid, loremipsum, std::strlen(loremipsum));
    });

    peer.peer_expired.connect(on_peer_expired);
    peer.packet_received.connect(on_packet_received);

    assert(peer.configure(configurator{}));

    // FIXME unable process multicast group
    //REQUIRE(peer1.add_discovery_target(p2p::inet4_addr_t{224, 0, 0, 1}, 4243));
    peer.add_discovery_target(TARGET_ADDR, DISCOVERY_PORT);

    worker(peer);

    p2p::algorithm::cleanup();

    return EXIT_SUCCESS;
}
