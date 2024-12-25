////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.14 Initial version.
//      2024.12.24 Refactored usage and parsing the command line.
////////////////////////////////////////////////////////////////////////////////
#include <pfs/argvapi.hpp>
#include <pfs/filesystem.hpp>
#include <pfs/fmt.hpp>
#include <pfs/integer.hpp>
#include <pfs/log.hpp>
#include <pfs/string_view.hpp>
#include <pfs/netty/startup.hpp>
#include <pfs/netty/posix/udp_receiver.hpp>
#include <pfs/netty/posix/udp_sender.hpp>
#include <chrono>
#include <limits>

using string_view = pfs::string_view;
using pfs::to_string;
static char const * TAG = "udp-demo";

#include "sender_routine.hpp"
#include "receiver_routine.hpp"

static void print_usage (pfs::filesystem::path const & programName
    , std::string const & errorString = std::string{})
{
    if (!errorString.empty())
        LOGE(TAG, "{}", errorString);

    fmt::println("Usage:\n\n"
        "{0} --help | -h\n"
        "{0} --sender [--interval=INTERVAL] [--max-count=COUNT] [--quit-only] --addr=ADDR [--port=PORT]\n\n"
        "{0} --receiver [--addr=ADDR] [--port=PORT] [--local-addr=ADDR]\n\n"

        "Options:\n\n"
        "--help | -h\n"
        "\tPrint this help and exit\n"
        "--sender\n"
        "\tRun as sender\n"
        "--receiver\n"
        "\tRun as receiver\n"
        "--interval=INTERVAL\n"
        "\tSend interval in milliseconds from 0 to 10000 (default is 1000 ms)\n"
        "--max-count=COUNT\n"
        "\tMaximum number of send iterations from 0 to 4294967295 (default is 10)\n"
        "--quit-only\n"
        "\tSend quit packet only by sender (need to force stop the receiver)\n"
        "--addr=ADDR\n"
        "\tSource address for receiver (default is 0.0.0.0), destination address for sender\n"
        "--port=PORT\n"
        "\tSource port for receiver, destination port for sender (default is 4242)\n"
        "--local-addr=ADDR\n"
        "\tLocal address for multicast receiver\n\n"

        "Examples:\n\n"
        "Run Multicast sender:\n"
        "  {0} --sender --addr=227.1.1.1 --port=4242\n\n"
        "Run Multicast receiver:\n"
        "  {0} --receiver --addr=227.1.1.1 --port=4242\n"
        , programName);
}

int main (int argc, char * argv[])
{
    netty::startup_guard netty_startup;

    bool is_sender = false; // false - for receiver, true - for sender
    pfs::optional<netty::inet4_addr> addr;
    pfs::optional<netty::inet4_addr> localAddr {0}; // INADDR_ANY
    std::uint16_t port = 4242;
    std::chrono::milliseconds interval {1000};
    std::uint32_t maxCount = 10;
    bool quitOnly = false;

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
            } else if (x.is_option("sender")) {
                is_sender = true;
            } else if (x.is_option("receiver")) {
                is_sender = false;
            } else if (x.is_option("addr") || x.is_option("local-addr")) {
                if (x.has_arg()) {
                    auto a = netty::inet4_addr::parse(x.arg());

                    if (!a) {
                        LOGE(TAG, "Bad address for '{}'", to_string(x.optname()));
                        return EXIT_FAILURE;
                    }

                    if (x.is_option("addr"))
                        addr = a;
                    else
                        localAddr = a;
                } else {
                    expectedArgError = true;
                }
            } else if (x.is_option("port")) {
                if (x.has_arg()) {
                    std::error_code ec;
                    port = pfs::to_integer(x.arg().begin(), x.arg().end(), std::uint16_t{1024}
                        , std::uint16_t{65535}, ec);

                    if (ec) {
                        LOGE(TAG, "Bad port: {}", ec.message());
                        return EXIT_FAILURE;
                    }
                } else {
                    expectedArgError = true;
                }
            } else if (x.is_option("interval")) {
                if (x.has_arg()) {
                    std::error_code ec;
                    auto t = pfs::to_integer(x.arg().begin(), x.arg().end(), int{0}, int{10000}, ec);

                    if (ec) {
                        LOGE(TAG, "Bad interval: {}", ec.message());
                        return EXIT_FAILURE;
                    }

                    interval = std::chrono::milliseconds{t};
                } else {
                    expectedArgError = true;
                }
            } else if (x.is_option("max-count")) {
                if (x.has_arg()) {
                    std::error_code ec;
                    auto n = pfs::to_integer(x.arg().begin(), x.arg().end(), std::uint32_t{0}
                        , std::numeric_limits<std::uint32_t>::max(), ec);

                    if (ec) {
                        LOGE(TAG, "Bad max-count: {}", ec.message());
                        return EXIT_FAILURE;
                    }

                    maxCount = n;
                } else {
                    expectedArgError = true;
                }
            } else if (x.is_option("quit-only")) {
                quitOnly = true;
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

    if (!addr) {
        if (is_sender) {
            LOGE(TAG, "No destination address specified");
            return EXIT_FAILURE;
        } else {
            *addr = netty::inet4_addr::any_addr_value;
        }
    }

    if (is_sender) {
        run_sender<netty::posix::udp_sender>(netty::socket4_addr{*addr, port}, *localAddr, interval
            , maxCount, quitOnly);
    } else {
        run_receiver<netty::posix::udp_receiver>(netty::socket4_addr{*addr, port}, *localAddr);
    }

    return EXIT_SUCCESS;
}
