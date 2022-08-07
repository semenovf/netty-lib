////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.09.13 Initial version
//      2021.11.01 New version using UDT.
////////////////////////////////////////////////////////////////////////////////
#define PFS__LOG_LEVEL 2
#include "pfs/log.hpp"
#include "pfs/netty/inet4_addr.hpp"
#include "pfs/netty/p2p/engine.hpp"
#include "pfs/netty/p2p/qt5/api.hpp"
#include "pfs/netty/p2p/udt/api.hpp"

static char loremipsum[] = "WORLD";

static char loremipsum1[] =
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

namespace fs = pfs::filesystem;
namespace p2p = netty::p2p;
static constexpr std::size_t PACKET_SIZE = 64;

using engine_t = p2p::engine<p2p::qt5::api, p2p::udt::api, PACKET_SIZE>;

namespace {
    const pfs::universal_id UUID = pfs::generate_uuid();

    std::chrono::milliseconds const DISCOVERY_TRANSMIT_INTERVAL {100};
    std::chrono::milliseconds const PEER_EXPIRATION_TIMEOUT {2000};
    std::chrono::milliseconds const POLL_INTERVAL {10};
    //std::chrono::milliseconds const POLL_INTERVAL {1000};

    const netty::inet4_addr TARGET_ADDR{227, 1, 1, 255};
    const netty::inet4_addr DISCOVERY_ADDR{};
    const std::uint16_t DISCOVERY_PORT{4242u};
}

struct configurator
{
    netty::inet4_addr discovery_address () const noexcept { return DISCOVERY_ADDR; }
    std::uint16_t discovery_port () const noexcept { return DISCOVERY_PORT; }
    std::chrono::milliseconds discovery_transmit_interval () const noexcept { return DISCOVERY_TRANSMIT_INTERVAL; }
    std::chrono::milliseconds expiration_timeout () const noexcept { return PEER_EXPIRATION_TIMEOUT; }
    std::chrono::milliseconds poll_interval () const noexcept { return POLL_INTERVAL; }
    netty::inet4_addr listener_address () const noexcept { return netty::inet4_addr{}; }
    std::uint16_t listener_port () const noexcept { return 4224u; }
    int backlog () const noexcept { return 10; }
    //std::size_t file_chunk_size () const noexcept { return 64 * 1024; }
};

void on_failure (std::string const & error)
{
    fmt::print(stderr, "!ERROR: {}\n", error);
}

void on_rookie_accepted (pfs::universal_id uuid
    , netty::inet4_addr const & addr
    , std::uint16_t port)
{
    LOG_TRACE_1("HELO: {} ({}:{})"
        , to_string(uuid)
        , to_string(addr)
        , port);
}

void on_peer_closed (pfs::universal_id uuid
    , netty::inet4_addr const & addr
    , std::uint16_t port)
{}

void worker (engine_t & engine)
{
    while (true) {
        engine.loop();
    }
}

int main (int argc, char * argv[])
{
    std::string program{argv[0]};
    pfs::filesystem::path file;
    pfs::crypto::sha256_digest sha256;

    if (argc > 1) {
        file = fs::utf8_decode(argv[1]);

        if (!fs::exists(file)) {
            LOGE("", "File does not exist: {}", file);
            return EXIT_FAILURE;
        }

        LOGD("", "Calculate digest for: {}", file);
        sha256 = pfs::crypto::sha256::digest(file);
        LOGD("", "Digest for: {}: {}", file, to_string(sha256));
    }

    fmt::print("My name is {}\n", to_string(UUID));

    if (!engine_t::startup())
        return EXIT_FAILURE;

    engine_t engine {UUID};

    engine.failure = on_failure;
    engine.rookie_accepted = on_rookie_accepted;
    engine.peer_closed = on_peer_closed;
    engine.writer_ready = [& engine, & file, & sha256] (pfs::universal_id peer_uuid
            , netty::inet4_addr const & addr
            , std::uint16_t port) {
        LOG_TRACE_1("WRITER READY: {} ({}:{})"
            , to_string(peer_uuid)
            , to_string(addr)
            , port);

        if (!file.empty()) {
            engine.send_file(peer_uuid, file, sha256, 0);
        }

        engine.send(peer_uuid, loremipsum, std::strlen(loremipsum));
    };

    engine.data_received = [& engine] (pfs::universal_id peer_uuid, std::string message) {
        if (message.size() > 20) {
            LOG_TRACE_1("Message received from {}: {}...{} ({}/{} characters (received/expected))"
                , to_string(peer_uuid)
                , message.substr(0, 20)
                , message.substr(message.size() - 20)
                , message.size()
                , std::strlen(loremipsum));
        } else {
            LOG_TRACE_1("Message received from {}: {} ({}/{} characters (received/expected))"
                , to_string(peer_uuid)
                , message
                , message.size()
                , std::strlen(loremipsum));
        }

        if (message != loremipsum) {
            LOGE("", "Received message and sample are different");
        } else {
            LOGI("", "Received message and sample are equal");
        }

        //engine.send(peer_uuid, loremipsum, std::strlen(loremipsum));
    };

    engine.file_credentials_received = [] (pfs::universal_id
        , netty::p2p::file_credentials) {
    };

    engine.data_dispatched = [] (engine_t::entity_id id) {
        LOG_TRACE_1("Message dispatched: {}", id);
    };

    auto success = engine.configure(configurator{});

    success = success
        && engine.set_option(p2p::option_enum::download_directory, fs::temp_directory_path())
        && engine.set_option(p2p::option_enum::auto_download, true);

    if (!success)
        return EXIT_FAILURE;

    engine.add_discovery_target(TARGET_ADDR, DISCOVERY_PORT);

    worker(engine);

    engine_t::cleanup();

    return EXIT_SUCCESS;
}
