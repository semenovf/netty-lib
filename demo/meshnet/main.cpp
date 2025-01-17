////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "meshnode.hpp"
#include <pfs/argvapi.hpp>
#include <pfs/filesystem.hpp>
#include <pfs/fmt.hpp>
#include <pfs/integer.hpp>
#include <pfs/log.hpp>
#include <pfs/string_view.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <pfs/netty/startup.hpp>
#include <atomic>
#include <signal.h>

using string_view = pfs::string_view;
using pfs::to_string;

static constexpr char const * TAG = "meshnet";
static constexpr std::uint16_t PORT = 4242;
static std::atomic_bool s_quit {false};

static void sigterm_handler (int /*sig*/)
{
    s_quit.store(true);
}

static void print_usage (pfs::filesystem::path const & programName
    , std::string const & errorString = std::string{})
{
    if (!errorString.empty())
        LOGE(TAG, "{}", errorString);

    fmt::println("Usage:\n\n"
        "{0} --help | -h\n"
        "{0} --node-addr=ADDR...\n\n"

        "Options:\n\n"
        "--help | -h\n"
        "\tPrint this help and exit\n"
        "--node-addr=ADDR\n"
        "\tPeer node address\n\n"

        "Example:\n\n"
        "Run with connection to 192.168.0.2:\n"
        "\t{0} --node-addr=192.168.0.2\n"
        , programName);
}

int main (int argc, char * argv[])
{
    signal(SIGINT, sigterm_handler);
    signal(SIGTERM, sigterm_handler);

    netty::startup_guard netty_startup;

    std::vector<netty::socket4_addr> _nodes;
    meshnet_node_t node;
    netty::inet4_addr listenerAddr = netty::inet4_addr{netty::inet4_addr::any_addr_value};
    node.add_listener(netty::socket4_addr{listenerAddr, PORT});

    auto commandLine = pfs::make_argvapi(argc, argv);
    auto programName = commandLine.program_name();
    auto commandLineIterator = commandLine.begin();

    if (commandLineIterator.has_more()) {
        while (commandLineIterator.has_more()) {
            auto x = commandLineIterator.next();
            auto expectedArgError = false;

            if (x.is_option("help") || x.is_option("h")) {
                print_usage(programName);
                return EXIT_SUCCESS;
            } else if (x.is_option("node-addr")) {
                if (x.has_arg()) {
                    auto addr = netty::inet4_addr::parse(x.arg());

                    if (!addr) {
                        LOGE(TAG, "Bad address for '{}'", to_string(x.optname()));
                        return EXIT_FAILURE;
                    }

                    node.connect_host(netty::socket4_addr{*addr, PORT});
                } else {
                    expectedArgError = true;
                }
            } else {
                LOGE(TAG, "Bad arguments. Try --help option.");
                return EXIT_FAILURE;
            }

            if (expectedArgError) {
                print_usage(programName, "Expected argument for " + to_string(x.optname()));
                return EXIT_FAILURE;
            }
        }
    } else {
        print_usage(programName);
        return EXIT_SUCCESS;
    }

    node.listen();

    while (!s_quit.load())
        node.step(std::chrono::milliseconds {10});

    return EXIT_SUCCESS;
}
