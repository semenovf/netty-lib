////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.09.13 Initial version
//      2021.11.01 New version using UDT.
//      2023.01.18 Version 2 started.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/log.hpp"
#include "pfs/string_view.hpp"
#include "pfs/netty/inet4_addr.hpp"
#include "pfs/netty/poller_types.hpp"
#include "pfs/netty/p2p/engine.hpp"
#include "pfs/netty/p2p/engine_traits.hpp"
#include "pfs/netty/p2p/posix/discovery_engine.hpp"
#include <chrono>

static char const * loremipsum =
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

using string_view = pfs::string_view;

static char const * TAG = "netty-p2p";
static constexpr std::size_t PACKET_SIZE = 64;

using engine_t = p2p::engine<p2p::posix::discovery_engine
    , p2p::default_engine_traits
    , p2p::file_transporter
    , PACKET_SIZE>;

static struct program_context {
    std::string program;
} __pctx;

static void print_usage ()
{
    fmt::print(stdout, "Usage\n{} --uuid=UUID --listener-saddr=ADDR:PORT\n", __pctx.program);
    fmt::print(stdout, "\t--discovery-saddr=ADDR:PORT\n");
    fmt::print(stdout, "\t--target-saddr=ADDR:PORT...\n");
}

int main (int argc, char * argv[])
{
    p2p::universal_id my_uuid;
    netty::socket4_addr discovery_saddr {netty::inet4_addr{}, 0};
    netty::socket4_addr listener_saddr {netty::inet4_addr{}, 0};
    std::vector<netty::socket4_addr> target_saddrs;

//     pfs::filesystem::path file;

    __pctx.program = fs::utf8_encode(fs::utf8_decode(argv[0]).filename());

    if (argc == 1) {
        print_usage();
        return EXIT_SUCCESS;
    }

    for (int i = 1; i < argc; i++) {
        if (string_view{"-h"} == argv[i] || string_view{"--help"} == argv[i]) {
            print_usage();
            return EXIT_SUCCESS;
        } else if (pfs::starts_with(string_view{argv[i]}, "--uuid=")) {
            my_uuid = pfs::from_string<pfs::universal_id>(std::string{argv[i] + 7});

            if (my_uuid == pfs::universal_id{}) {
                LOGE("", "Bad UUID");
                return EXIT_FAILURE;
            }
        } else if (pfs::starts_with(string_view{argv[i]}, "--listener-saddr=")) {
            char const * saddr_value = argv[i] + 17;
            auto res = netty::socket4_addr::parse(saddr_value);

            if (!res.first) {
                LOGE(TAG, "Bad address for main listener: {}", saddr_value);
                return EXIT_FAILURE;
            }

            listener_saddr = res.second;
        } else if (pfs::starts_with(string_view{argv[i]}, "--discovery-saddr=")) {
            char const * saddr_value = argv[i] + 18;
            auto res = netty::socket4_addr::parse(saddr_value);

            if (!res.first) {
                LOGE(TAG, "Bad discovery receiver address : {}", saddr_value);
                return EXIT_FAILURE;
            }

            discovery_saddr = res.second;
        } else if (pfs::starts_with(string_view{argv[i]}, "--target-saddr=")) {
            char const * saddr_value = argv[i] + 15;
            auto res = netty::socket4_addr::parse(saddr_value);

            if (!res.first) {
                LOGE(TAG, "Bad discovery target address: {}", saddr_value);
                return EXIT_FAILURE;
            }

            target_saddrs.push_back(std::move(res.second));
        } else {
            auto arglen = std::strlen(argv[i]);

            if (arglen > 0 && argv[i][0] == '-') {
                LOGE(TAG, "Bad option: {}", argv[i]);
                return EXIT_FAILURE;
            }
        }
    }

    if (my_uuid == p2p::universal_id{}) {
        LOGE(TAG, "UUID must be specified");
        return EXIT_FAILURE;
    }

    if (discovery_saddr.port == 0) {
        LOGE(TAG, "Discovery address must be specified");
        return EXIT_FAILURE;
    }

    if (listener_saddr.port == 0) {
        LOGE(TAG, "Listener address must be specified");
        return EXIT_FAILURE;
    }

    engine_t::options opts = engine_t::default_options();
    opts.discovery.timestamp_error_limit = std::chrono::milliseconds{500};

    // Port on which server will accept incoming connections (readers)
    opts.discovery.host_port = listener_saddr.port;

    opts.filetransporter.download_directory = pfs::filesystem::temp_directory_path();
    opts.filetransporter.download_progress_granularity = 1;
    opts.filetransporter.file_chunk_size = 64 * 1024;
    opts.filetransporter.max_file_size = 0x7ffff000;
    opts.filetransporter.remove_transient_files_on_error = false;

    LOGI(TAG, "My name: {}", my_uuid);
    LOGI(TAG, "Listener address: {}", to_string(listener_saddr));
    LOGI(TAG, "Discovery address: {}", to_string(discovery_saddr));

    engine_t engine {my_uuid, listener_saddr, opts};
    engine.add_receiver(discovery_saddr);

    for (auto t: target_saddrs) {
        engine.add_target(t, std::chrono::seconds{2}, std::chrono::seconds{5});
    }

    engine.on_failure = [] (netty::error const & err) {
        fmt::println(stderr, "ERROR: {}", err.what());
    };

    engine.peer_discovered = [] (p2p::universal_id contact_id
        , netty::inet4_addr addr
        , std::uint16_t port
        , std::chrono::milliseconds const & timediff) {

        LOGD(TAG, "Peer available: {}@{}:{}, time difference: {}"
            , contact_id, to_string(addr), port, timediff);
    };

    engine.peer_timediff = [& engine] (p2p::universal_id peer_uuid
        , std::chrono::milliseconds const & diff) {
        LOGD(TAG, "Peer time difference: {}: {}", peer_uuid, diff);

        engine.send(peer_uuid, loremipsum, std::strlen(loremipsum));
    };

    engine.peer_expired = [] (p2p::universal_id peer_uuid
        , netty::inet4_addr addr, std::uint16_t port) {
        LOGD(TAG, "Peer expired: {}@{}:{}", peer_uuid, to_string(addr), port);
    };

    engine.writer_ready = [] (p2p::universal_id peer_uuid
        , netty::inet4_addr addr, std::uint16_t port) {
        LOGD(TAG, "Writer ready: {}@{}:{}", peer_uuid, to_string(addr), port);

            // FIXME
//         if (!file.empty()) {
//             engine.send_file(peer_uuid, file);
//         }
    };

    engine.writer_closed = [] (p2p::universal_id peer_uuid
        , netty::inet4_addr addr, std::uint16_t port) {
        LOGD(TAG, "Writer closed: {}@{}:{}", peer_uuid, to_string(addr), port);
    };

    engine.reader_ready = [] (p2p::universal_id peer_uuid
        , netty::inet4_addr addr, std::uint16_t port) {
        LOGD(TAG, "Reader ready: {}@{}:{}", peer_uuid, to_string(addr), port);
    };

    engine.reader_closed = [] (p2p::universal_id peer_uuid
        , netty::inet4_addr addr, std::uint16_t port) {
        LOGD(TAG, "Reader closed: {}@{}:{}", peer_uuid, to_string(addr), port);
    };

    engine.data_received = [] (p2p::universal_id sender_uuid, std::vector<char> data) {
        LOGD(TAG, "Data received from: {}, {} bytes", sender_uuid, data.size());

        if (data.size() > 20) {
            LOG_TRACE_1("Message received from {}: {}...{} ({}/{} characters (received/expected))"
                , sender_uuid
                , data.substr(0, 20)
                , data.substr(data.size() - 20)
                , data.size()
                , std::strlen(loremipsum));
        } else {
            LOG_TRACE_1("Message received from {}: {} ({}/{} characters (received/expected))"
                , to_string(sender_uuid)
                , data
                , data.size()
                , std::strlen(loremipsum));
        }

        if (data != loremipsum) {
            LOGE(TAG, "Received message and sample are different");
        } else {
            LOGI(TAG, "Received message and sample are equal");
        }
    };

    engine.download_progress = [] (p2p::universal_id sender_id
        , pfs::universal_id file_id
        , p2p::filesize_t downloaded_size
        , p2p::filesize_t total_size) {
        LOGD(TAG, "Download progress: {}, file_id={}: {}/{} bytes ({} %)", sender_id
            , file_id, downloaded_size, total_size
            , (static_cast<float>(downloaded_size) / total_size) * 100);
    };

    engine.download_complete = [] (p2p::universal_id sender_id
        , pfs::universal_id file_id, pfs::filesystem::path const & path
        , bool success) {
        LOGD(TAG, "Download complete: {}, {} ({}): {}"
            , sender_id, file_id, path, success ? "SUCCESS" : "FAILURE");
    };

    engine.download_interrupted = [] (p2p::universal_id sender_id
        , pfs::universal_id file_id) {
        LOGD(TAG, "Download interrupted: {}, file_id={}", sender_id, file_id);
    };

    try {
        if (!engine.start())
            return EXIT_FAILURE;

        while (true)
            engine.loop();
    } catch (pfs::error ex) {
        LOGE(TAG, "{}", ex.what());
    }

    return EXIT_SUCCESS;
}
