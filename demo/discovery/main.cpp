////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/fmt.hpp"
#include "pfs/integer.hpp"
#include "pfs/log.hpp"
#include "pfs/string_view.hpp"
#include "pfs/netty/startup.hpp"
#include "pfs/netty/p2p/discovery_engine.hpp"
#include "pfs/netty/p2p/discovery_engine.hpp"
#include "pfs/netty/p2p/posix/discovery_engine.hpp"

#if NETTY__QT5_ENABLED
#   include "pfs/netty/p2p/qt5/discovery_engine.hpp"
#endif

#include <vector>

static char const * TAG = "DISCOVERY";

using string_view = pfs::string_view;
using discovery_engine = netty::p2p::discovery_engine<netty::p2p::posix::discovery_engine>;

static struct program_context {
    std::string program;
} __pctx;

static void print_usage ()
{
    fmt::print(stdout, "Usage\n\t{} [--local-addr=ADDR] --listener-saddr=ADDR:PORT... --target-saddr=ADDR:PORT...\n", __pctx.program);
//     fmt::print(stdout, "--target-addr=ADDR\n\tSource address for receiver, destination address for sender\n");
//     fmt::print(stdout, "--port=PORT\n\tSource port for receiver, destination port for sender\n");
}

int main (int argc, char * argv[])
{
    netty::startup_guard netty_startup;

    std::vector<std::string> listener_saddr_values;
    std::vector<std::string> target_saddr_values;
    netty::inet4_addr local_addr;

    __pctx.program = argv[0];

    if (argc == 1) {
        print_usage();
        return EXIT_SUCCESS;
    }

    for (int i = 1; i < argc; i++) {
        if (string_view{"-h"} == argv[i] || string_view{"--help"} == argv[i]) {
            print_usage();
            return EXIT_SUCCESS;
        } else if (string_view{argv[i]}.starts_with("--local-addr=")) {
            auto res = netty::inet4_addr::parse(argv[i] + 13);

            if (!res.first) {
                LOGE(TAG, "Bad local address");
                return EXIT_FAILURE;
            }
            local_addr = std::move(res.first);
        } else if (string_view{argv[i]}.starts_with("--listener-saddr=")) {
            listener_saddr_values.push_back(std::string{argv[i] + 17});
        } else if (string_view{argv[i]}.starts_with("--target-saddr=")) {
            target_saddr_values.push_back(std::string{argv[i] + 15});
        } else {
            auto arglen = std::strlen(argv[i]);

            if (arglen > 0 && argv[i][0] == '-') {
                LOGE(TAG, "Bad option: {}", argv[i]);
                return EXIT_FAILURE;
            }
        }
    }

    if (listener_saddr_values.empty()) {
        LOGE(TAG, "No listeners specified");
        return EXIT_FAILURE;
    }

    if (target_saddr_values.empty()) {
        LOGE(TAG, "No targets specified");
        return EXIT_FAILURE;
    }

    auto host_uuid = pfs::generate_uuid();

    discovery_engine::options opts;
    opts.host_port = 42043;
    discovery_engine discovery {host_uuid, std::move(opts)};

    for (auto l: listener_saddr_values) {
        auto res = netty::socket4_addr::parse(l);

        if (!res.first) {
            LOGE(TAG, "Bad socket address for receiver: {}", l);
            return EXIT_FAILURE;
        }

        discovery.add_receiver(res.second, local_addr);
    }

    for (auto t: target_saddr_values) {
        auto res = netty::socket4_addr::parse(t);

        if (!res.first) {
            LOGE(TAG, "Bad socket address for target: {}", t);
            return EXIT_FAILURE;
        }

        // FIXME
//         discovery.add_target(res.second, local_addr);
    }

    discovery.on_error = [] (std::string const & errstr) {
        LOGE(TAG, "{}", errstr);
    };

    discovery.peer_discovered = [] (netty::p2p::universal_id peer_uuid
        , netty::socket4_addr saddr, std::chrono::milliseconds const & timediff) {

        LOGD(TAG, "Peer discovered: {} {} (time diff={})"
            , peer_uuid, to_string(saddr), timediff);
    };

    discovery.peer_timediff = [] (netty::p2p::universal_id peer_uuid
        , std::chrono::milliseconds const & timediff) {
        LOGD(TAG, "Peer time diff: {} (time diff={})", peer_uuid, timediff);
    };

    discovery.peer_expired = [] (netty::p2p::universal_id peer_uuid
        , netty::socket4_addr saddr) {
        LOGD(TAG, "Peer expired: {} {}", peer_uuid, to_string(saddr));
    };

    while (true)
        discovery.discover(std::chrono::milliseconds{100});

    return EXIT_SUCCESS;
}
