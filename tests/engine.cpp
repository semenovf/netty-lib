////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// License: see LICENSE file
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define PFS_NET_P2P__TRACE_LEVEL 3
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "pfs/net/inet4_addr.hpp"
#include "pfs/net/p2p/engine.hpp"
#include "pfs/net/p2p/qt5/api.hpp"
#include "pfs/net/p2p/udt/api.hpp"
#include <atomic>
#include <thread>

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
using controller           = pfs::net::p2p::controller;
using discovery_socket_api = pfs::net::p2p::qt5::api;
using reliable_socket_api  = pfs::net::p2p::udt::api;
static constexpr std::size_t DEFAULT_PACKET_SIZE = 64;

using engine = pfs::net::p2p::engine<
      discovery_socket_api
    , reliable_socket_api
    , DEFAULT_PACKET_SIZE>;
} // namespace p2p

namespace {
    pfs::uuid_t const PEER1_UUID = "01FH7H6YJB8XK9XNNZYR0WYDJ1"_uuid;
    pfs::uuid_t const PEER2_UUID = "01FH7HB19B9T1CTKE5AXPTN74M"_uuid;

    std::chrono::milliseconds const DISCOVERY_TRANSMIT_INTERVAL {100};
    std::chrono::milliseconds const PEER_EXPIRATION_TIMEOUT {500};
    std::chrono::milliseconds const POLL_INTERVAL {10};
    //std::chrono::milliseconds const POLL_INTERVAL {1000};

    std::atomic_bool QUIT_PEER1 {false};
    std::atomic_bool QUIT_PEER2 {false};
}

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

    if (uuid == PEER2_UUID)
        QUIT_PEER2 = true;
}

void on_peer_expired (pfs::uuid_t uuid
    , pfs::net::inet4_addr const & addr
    , std::uint16_t port)
{
    TRACE_1("EXPIRED: {} ({}:{})"
        , std::to_string(uuid)
        , std::to_string(addr)
        , port);

    QUIT_PEER1 = true;
}

void worker (p2p::engine & peer)
{
    TRACE_1("Peer started: {}", std::to_string(peer.uuid()));

    while (true) {
        peer.loop();

        if (peer.uuid() == PEER1_UUID && QUIT_PEER1)
            break;

        if (peer.uuid() == PEER2_UUID && QUIT_PEER2)
            break;
    }

    TRACE_1("Peer finished: {}", std::to_string(peer.uuid()));
}

struct configurator1
{
    p2p::inet4_addr discovery_address () const noexcept { { return p2p::inet4_addr{127, 0, 0, 1}; } }
    std::uint16_t   discovery_port () const noexcept { return 4242u; }
    std::chrono::milliseconds discovery_transmit_interval () const noexcept { return DISCOVERY_TRANSMIT_INTERVAL; }
    std::chrono::milliseconds expiration_timeout () const noexcept { return PEER_EXPIRATION_TIMEOUT; }
    std::chrono::milliseconds poll_interval () const noexcept { return POLL_INTERVAL; }
    p2p::inet4_addr listener_address () const noexcept { return p2p::inet4_addr{127, 0, 0, 1}; }
    std::uint16_t   listener_port () const noexcept { return 42224u; }
    int backlog () const noexcept { return 10; }
};

struct configurator2
{
    p2p::inet4_addr discovery_address () const noexcept { { return p2p::inet4_addr{127, 0, 0, 1}; } }
    std::uint16_t   discovery_port () const noexcept { return 4243u; }
    std::chrono::milliseconds discovery_transmit_interval () const noexcept { return DISCOVERY_TRANSMIT_INTERVAL; }
    std::chrono::milliseconds expiration_timeout () const noexcept { return PEER_EXPIRATION_TIMEOUT; }
    std::chrono::milliseconds poll_interval () const noexcept { return POLL_INTERVAL; }
    p2p::inet4_addr listener_address () const noexcept { return p2p::inet4_addr{127, 0, 0, 1}; }
    std::uint16_t   listener_port () const noexcept { return 42324u; }
    int backlog () const noexcept { return 10; }
};

TEST_CASE("HELO")
{
    REQUIRE(p2p::engine::startup());

    std::thread peer1_worker;
    std::thread peer2_worker;

    peer1_worker = std::thread{[] {
        p2p::controller controller1;
        p2p::engine peer1 {controller1, PEER1_UUID};
        controller1.failure.connect(on_failure);
        controller1.rookie_accepted.connect(on_rookie_accepted);
        controller1.writer_ready.connect(on_writer_ready);
        controller1.peer_expired.connect(on_peer_expired);

        REQUIRE(peer1.configure(configurator1{}));

        peer1.add_discovery_target(p2p::inet4_addr{127, 0, 0, 1}, 4243u);

        worker(peer1);
    }};

    peer2_worker = std::thread{[] {
        p2p::controller controller2;
        p2p::engine peer2 {controller2, PEER2_UUID};
        controller2.failure.connect(on_failure);
        controller2.rookie_accepted.connect(on_rookie_accepted);
        controller2.writer_ready.connect(on_writer_ready);
        controller2.peer_expired.connect(on_peer_expired);

        REQUIRE(peer2.configure(configurator2{}));

        peer2.add_discovery_target(p2p::inet4_addr{127, 0, 0, 1}, 4242);

        worker(peer2);
    }};

    peer1_worker.join();
    peer2_worker.join();

    p2p::engine::cleanup();
}
