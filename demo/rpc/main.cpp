////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/fmt.hpp"
#include "pfs/log.hpp"
#include "pfs/string_view.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include "pfs/netty/startup.hpp"
#include "pfs/netty/p2p/remote_file.hpp"
// #include <cstdlib>
// #include <cstring>
#include <string>
// #include <utility>
// #include <vector>

static char const * TAG = "pfs.netty.p2p";

static struct program_context {
    std::string program;
} __pctx;

static void print_usage ()
{
    fmt::print(stdout, "Usage\n\t{} --provider=ip4_addr:port\n", __pctx.program);
}

int main (int argc, char * argv[])
{
    netty::startup_guard netty_startup;

    netty::socket4_addr provider_saddr;

    __pctx.program = argv[0];

    if (argc == 1) {
        print_usage();
        return EXIT_SUCCESS;
    }

    for (int i = 1; i < argc; i++) {
        if (string_view{"-h"} == argv[i] || string_view{"--help"} == argv[i]) {
            print_usage();
            return EXIT_SUCCESS;
        } else if (starts_with(string_view{argv[i]}, "--provider=")) {
            auto res = netty::socket4_addr::parse(argv[i] + 11);

            if (!res.first) {
                LOGE(TAG, "Bad content provider address");
                return EXIT_FAILURE;
            }

            provider_saddr = res.second;
        } else {
            auto arglen = std::strlen(argv[i]);

            if (arglen > 0 && argv[i][0] == '-') {
                LOGE(TAG, "Bad option: {}", argv[i]);
                return EXIT_FAILURE;
            }
        }
    }

    auto path = netty::p2p::select_remote_file(provider_saddr);

    if (path.empty()) {
        LOGI(TAG, "File not selected");
        return EXIT_SUCCESS;
    }

    LOGD(TAG, "Remote path selected: {}", path.uri);

//     netty::p2p::remote_file rfile = netty::p2p::remote_file::open;



//     client_socket_engine::options opts = client_socket_engine::default_options();
//     client_socket_engine client_socket {opts};
//
//     client_socket.connected = [& client_socket] () {
//         LOGD(TAG, "CONNECTED");
//
//         packet pkt1 {
//             R"_({"jsonrpc": "2.0", "method": "hello", "params": {"greeting": "Hello, World!"}})_"
//         };
//
//         client_socket.send(pkt1);
//     };
//
//     client_socket.disconnected = [] {
//         LOGD(TAG, "DISCONNECTED");
//     };
//
//     if (client_socket.connect(server_saddr)) {
//         client_socket_engine::loop_result rs = client_socket_engine::loop_result::success;
//
//         while (rs == client_socket_engine::loop_result::success) {
//             rs = client_socket.loop();
//         }
//
//         if (rs != client_socket_engine::loop_result::success) {
//             LOGD(TAG, "Client socket stopped by reason: {}"
//                 , rs == client_socket_engine::loop_result::disconnected
//                     ? "DISCONNECTED"
//                     : rs == client_socket_engine::loop_result::connection_refused
//                         ? "CONNECTION REFUSED"
//                         : "UNKNOWN");
//         }
//     }

    return EXIT_SUCCESS;
}
