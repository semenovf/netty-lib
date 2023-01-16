////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.14 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/fmt.hpp"
#include "pfs/integer.hpp"
#include "pfs/log.hpp"
#include "pfs/string_view.hpp"
#include "pfs/netty/startup.hpp"

static char const * TAG = "UDP";

#include "sender_routine.hpp"
#include "receiver_routine.hpp"

using string_view = pfs::string_view;

static struct program_context {
    std::string program;
} __pctx;

static void print_usage ()
{
    fmt::print(stdout, "Usage\n\t{} --sender | --receiver --addr=ADDR --port=PORT [--local-addr=ADDR]\n", __pctx.program);
    fmt::print(stdout, "--sender\n\tRun as sender\n");
    fmt::print(stdout, "--receiver\n\tRun as receiver\n");
    fmt::print(stdout, "--addr=ADDR\n\tSource address for receiver, destination address for sender\n");
    fmt::print(stdout, "--port=PORT\n\tSource port for receiver, destination port for sender\n");
    fmt::print(stdout, "--local_addr=ADDR\n\tLocal address for multicast receiver\n");
    fmt::print(stdout, "\nRun Multicast sender\n\t{} --sender --addr=227.1.1.1 --port=4242\n", __pctx.program);
    fmt::print(stdout, "\nRun Multicast receiver\n\t{} --receiver --addr=227.1.1.1 --port=4242\n", __pctx.program);
}

int main (int argc, char * argv[])
{
    netty::startup_guard netty_startup;

    bool is_sender = false; // false - for receiver, true - for sender
    std::string addr_value;
    std::string local_addr_value;
    std::string port_value;

    __pctx.program = argv[0];

    if (argc == 1) {
        print_usage();
        return EXIT_SUCCESS;
    }

    for (int i = 1; i < argc; i++) {
        if (string_view{"-h"} == argv[i] || string_view{"--help"} == argv[i]) {
            print_usage();
            return EXIT_SUCCESS;
        } else if (string_view{"--sender"} == argv[i]) {
            is_sender = true;
        } else if (string_view{"--receiver"} == argv[i]) {
            is_sender = false;
        } else if (string_view{argv[i]}.starts_with("--addr=")) {
            addr_value = std::string{argv[i] + 7};
        } else if (string_view{argv[i]}.starts_with("--local-addr=")) {
            local_addr_value = std::string{argv[i] + 13};
        } else if (string_view{argv[i]}.starts_with("--port=")) {
            port_value = std::string{argv[i] + 7};
        } else {
            auto arglen = std::strlen(argv[i]);

            if (arglen > 0 && argv[i][0] == '-') {
                LOGE(TAG, "Bad option: {}", argv[i]);
                return EXIT_FAILURE;
            }
        }
    }

    if (addr_value.empty()) {
        LOGE(TAG, "No address (source/destination) specified");
        return EXIT_FAILURE;
    }

    auto res = netty::inet4_addr::parse(addr_value);

    if (!res.first) {
        LOGE(TAG, "Bad address");
        return EXIT_FAILURE;
    }

    auto addr = res.second;
    std::uint16_t port = 4242;

    if (!port_value.empty()) {
        std::error_code ec;
        port = pfs::to_integer(port_value.begin(), port_value.end()
            , std::uint16_t{1024}, std::uint16_t{65535}, ec);

        if (ec) {
            LOGE(TAG, "Bad port");
            return EXIT_FAILURE;
        }
    }

    netty::inet4_addr local_addr{0}; // INADDR_ANY

    if (!local_addr_value.empty()) {
        auto res = netty::inet4_addr::parse(local_addr_value);

        if (!res.first) {
            LOGE(TAG, "Bad local address");
            return EXIT_FAILURE;
        }

        local_addr = res.second;
    }

    if (is_sender) {
        run_sender(netty::socket4_addr{addr, port}, local_addr);
    } else {
        run_receiver(netty::socket4_addr{addr, port}, local_addr);
    }

    return EXIT_SUCCESS;
}
