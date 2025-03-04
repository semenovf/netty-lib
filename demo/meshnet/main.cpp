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
#include <cstdlib>
#include <ctime>
#include <signal.h>

using string_view = pfs::string_view;
using pfs::to_string;

static constexpr char const * TAG = "meshnet";
static constexpr std::uint16_t PORT = 4242;
static std::atomic_bool quit_flag {false};

static void sigterm_handler (int /*sig*/)
{
    quit_flag.store(true);
}

static void print_usage (pfs::filesystem::path const & programName
    , std::string const & errorString = std::string{})
{
    if (!errorString.empty())
        LOGE(TAG, "{}", errorString);

    fmt::println("Usage:\n\n"
        "{0} --help | -h\n"
        "{0} --id=NODE_ID [--nat] --node-addr=ADDR...\n\n"

        "Options:\n\n"
        "--help | -h\n"
        "\tPrint this help and exit\n"
        "--id=NODE_ID\n"
        "\tThis node identifier\n\n"
        "--nat\n"
        "\tThis node is behind NAT\n\n"
        "--gw\n"
        "\tThis node is a gateway\n\n"
        "--node-addr=ADDR\n"
        "\tNeighbor node address\n\n"

        "Example:\n\n"
        "Run with connection to 192.168.0.2:\n"
        "\t{0} --id=01JJP9YBH0TXV3HFQ3B10BXXXW --node-addr=192.168.0.2\n"
        , programName);
}

// Check node specilizations
void dumb ()
{
    bare_meshnet_node_t {pfs::generate_uuid(), false, false, std::make_shared<bare_meshnet_node_t::callback_suite>()};
    nopriority_meshnet_node_t {pfs::generate_uuid(), false, false, std::make_shared<nopriority_meshnet_node_t::callback_suite>()};
    priority_meshnet_node_t {pfs::generate_uuid(), false, false, std::make_shared<priority_meshnet_node_t::callback_suite>()};
}

int main (int argc, char * argv[])
{
    signal(SIGINT, sigterm_handler);
    signal(SIGTERM, sigterm_handler);

    auto behind_nat = netty::patterns::meshnet::behind_nat_enum::no;
    auto is_gateway = netty::patterns::meshnet::gateway_enum::no;
    std::vector<netty::socket4_addr> neighbor_node_saddrs;
    pfs::optional<node_pool_t::node_id> node_id_opt;

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
            } else if (x.is_option("id")) {
                if (x.has_arg()) {
                    node_id_opt = node_pool_t::node_id_traits::parse(x.arg().data()
                        , x.arg().size());

                    if (!node_id_opt) {
                        LOGE(TAG, "Bad node identifier");
                        return EXIT_FAILURE;
                    }
                } else {
                    expectedArgError = true;
                }
            } else if (x.is_option("node-addr")) {
                if (x.has_arg()) {
                    auto addr = netty::inet4_addr::parse(x.arg());

                    if (!addr) {
                        LOGE(TAG, "Bad address for '{}'", to_string(x.optname()));
                        return EXIT_FAILURE;
                    }

                    neighbor_node_saddrs.push_back(netty::socket4_addr{*addr, PORT});
                } else {
                    expectedArgError = true;
                }
            } else if (x.is_option("nat")) {
                behind_nat = netty::patterns::meshnet::behind_nat_enum::yes;
            } else if (x.is_option("gw")) {
                is_gateway = is_gateway = netty::patterns::meshnet::gateway_enum::yes;
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

    if (!node_id_opt) {
        LOGE(TAG, "Node identifier must be specified");
        return EXIT_FAILURE;
    }

    if (neighbor_node_saddrs.empty()) {
        LOGE(TAG, "No neighbor node address specified");
        return EXIT_FAILURE;
    }

    // For priority randoimization
    std::srand(std::time({}));

    netty::startup_guard netty_startup;
    auto callbacks = std::make_shared<node_pool_t::callback_suite>();

    callbacks->on_channel_established = [] (node_t::node_id id, bool is_gateway) {
        auto node_type = is_gateway ? "gateway node" : "regular node";
        LOGD(TAG, "Channel established with {}: {}", node_type, to_string(id));
    };

    callbacks->on_channel_destroyed = [] (node_t::node_id id) {
        LOGD(TAG, "Channel destroyed with {}", to_string(id));
    };

    node_pool_t node_pool {*node_id_opt, behind_nat, is_gateway, callbacks};

    std::vector<netty::socket4_addr> listener_saddrs {
        netty::socket4_addr{netty::inet4_addr {netty::inet4_addr::any_addr_value}, PORT}
    };
    auto cid = node_pool.add_node<node_t>(listener_saddrs);

    node_pool.listen();

    for (auto const & saddr: neighbor_node_saddrs)
        node_pool.connect_host(cid, saddr);

    while (!quit_flag.load())
        node_pool.step(std::chrono::milliseconds {10});

    return EXIT_SUCCESS;
}
