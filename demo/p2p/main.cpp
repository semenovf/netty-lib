////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.09.13 Initial version
//      2021.11.01 New version using UDT.
////////////////////////////////////////////////////////////////////////////////
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
    std::chrono::milliseconds const DISCOVERY_TRANSMIT_INTERVAL {1000};
    std::chrono::milliseconds const PEER_EXPIRATION_TIMEOUT {2000};
    std::chrono::milliseconds const POLL_INTERVAL {10};
    //std::chrono::milliseconds const POLL_INTERVAL {1000};

    const netty::inet4_addr TARGET_ADDR{227, 1, 1, 255};
    const netty::inet4_addr DISCOVERY_ADDR{};
    const std::uint16_t DISCOVERY_PORT{4242u};
}

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
    try {
        while (true) {
            engine.loop();
        }
    } catch (pfs::error ex) {
        on_failure(tr::f_("!!!EXCEPTION: {}", ex.what()));
    }
}

void print_usage (char const * program)
{
    fmt::print(stdout, "Usage\n\t{} [--uuid UUID] [FILE]\n", program);
}

int main (int argc, char * argv[])
{
    auto my_uuid = pfs::generate_uuid();
    pfs::filesystem::path file;
    pfs::crypto::sha256_digest sha256;

    bool success = true;

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            if (std::string{"-h"} == argv[i] || std::string{"--help"} == argv[i]) {
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            } else if (std::string{"--uuid"} == argv[i]) {
                if (++i >= argc) {
                    success = false;
                    break;
                }

                my_uuid = pfs::from_string<pfs::universal_id>(argv[i]);

                if (my_uuid == pfs::universal_id{}) {
                    LOGE("", "Bad UUID");
                    success = false;
                }
            } else {
                auto arglen = std::strlen(argv[i]);

                if (arglen > 0 && argv[i][0] == '-') {
                    LOGE("", "Bad option");
                    success = false;
                } else {
                    file = fs::utf8_decode(argv[i]);
                }
            }
        }
    }

    if (!success) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (!file.empty()) {
        if (!fs::exists(file)) {
            LOGE("", "File does not exist: {}", file);
            return EXIT_FAILURE;
        }
    }

    fmt::print("My name is {}\n", to_string(my_uuid));

    engine_t engine {my_uuid};

    engine.failure = on_failure;
    engine.peer_discovered = on_rookie_accepted;
    engine.peer_closed = on_peer_closed;
    engine.writer_ready = [& engine, & file, & sha256] (pfs::universal_id peer_uuid
            , netty::inet4_addr const & addr
            , std::uint16_t port) {
        LOG_TRACE_1("WRITER READY: {} ({}:{})"
            , to_string(peer_uuid)
            , to_string(addr)
            , port);

        if (!file.empty()) {
            engine.send_file(peer_uuid, file);
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

    engine.file_credentials_received = [] (pfs::universal_id sender
        , netty::p2p::file_credentials const & fc) {

        LOG_TRACE_1("###--- File credentials received from {}: {}"
            " (file name: {}; size: {})"
            , sender
            , fc.fileid
            , fc.filename
            , fc.filesize);

    };

    engine.data_dispatched = [] (engine_t::entity_id id) {
        LOG_TRACE_1("Message dispatched: {}", id);
    };

    engine.download_progress = [] (pfs::universal_id receiver_id
        , pfs::universal_id fileid
        , p2p::filesize_t downloaded_size
        , p2p::filesize_t total_size) {

        LOG_TRACE_2("Receiver {}: {}: {}%"
            , receiver_id
            , fileid
            , 100 * static_cast<float>(downloaded_size) / total_size);
    };

    success = success
        && engine.set_option(engine_t::option_enum::download_directory, fs::temp_directory_path())
        //&& engine.set_option(engine_t::option_enum::auto_download, true)
        && engine.set_option(engine_t::option_enum::discovery_address
            , netty::socket4_addr{DISCOVERY_ADDR, DISCOVERY_PORT})
        && engine.set_option(engine_t::option_enum::transmit_interval
            , DISCOVERY_TRANSMIT_INTERVAL)
        && engine.set_option(engine_t::option_enum::expiration_timeout
            , PEER_EXPIRATION_TIMEOUT)
        && engine.set_option(engine_t::option_enum::poll_interval
            , POLL_INTERVAL)
        && engine.set_option(engine_t::option_enum::listener_address
            , netty::socket4_addr{netty::inet4_addr{}, 4224})
        && engine.set_option(engine_t::option_enum::file_chunk_size
            , 16 * 1024);

    if (!success)
        return EXIT_FAILURE;

    if (engine.start()) {
        engine.add_discovery_target(TARGET_ADDR, DISCOVERY_PORT);
        worker(engine);
    }

    return EXIT_SUCCESS;
}
