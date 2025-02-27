////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "reliable_delivery_engine.hpp"
// #include <pfs/argvapi.hpp>
// #include <pfs/filesystem.hpp>
// #include <pfs/fmt.hpp>
// #include <pfs/integer.hpp>
// #include <pfs/log.hpp>
// #include <pfs/string_view.hpp>
// #include <pfs/netty/socket4_addr.hpp>
// #include <pfs/netty/startup.hpp>
#include <atomic>
#include <cstdlib>
// #include <ctime>
#include <signal.h>

// using string_view = pfs::string_view;
// using pfs::to_string;

static constexpr char const * TAG = "reliable-delivery";
// static constexpr std::uint16_t PORT = 4242;
static std::atomic_bool quit_flag {false};

static void sigterm_handler (int /*sig*/)
{
    quit_flag.store(true);
}

// static void print_usage (pfs::filesystem::path const & programName
//     , std::string const & errorString = std::string{})
// {
//     if (!errorString.empty())
//         LOGE(TAG, "{}", errorString);
//
//     fmt::println("Usage:\n\n"
//         "{0} --help | -h\n"
//         "{0} --id=NODE_ID --node-addr=ADDR...\n\n"
//
//         "Options:\n\n"
//         "--help | -h\n"
//         "\tPrint this help and exit\n"
//         "--id=NODE_ID\n"
//         "\tThis node identifier\n\n"
//         "--node-addr=ADDR\n"
//         "\tNeighbor node address\n\n"
//
//         "Example:\n\n"
//         "Run with connection to 192.168.0.2:\n"
//         "\t{0} --id=01JJP9YBH0TXV3HFQ3B10BXXXW --node-addr=192.168.0.2\n"
//         , programName);
// }

int main (int argc, char * argv[])
{
    signal(SIGINT, sigterm_handler);
    signal(SIGTERM, sigterm_handler);

    // std::vector<netty::socket4_addr> neighbor_node_saddrs;
    // pfs::optional<meshnet_node_t::node_id> node_id_opt;
    //
    // auto commandLine = pfs::make_argvapi(argc, argv);
    // auto programName = commandLine.program_name();
    // auto commandLineIterator = commandLine.begin();
    //
    // if (commandLineIterator.has_more()) {
    //     while (commandLineIterator.has_more()) {
    //         auto x = commandLineIterator.next();
    //         auto expectedArgError = false;
    //
    //         if (x.is_option("help") || x.is_option("h")) {
    //             print_usage(programName);
    //             return EXIT_SUCCESS;
    //         } else if (x.is_option("id")) {
    //             if (x.has_arg()) {
    //                 node_id_opt = meshnet_node_t::node_idintifier_traits::parse(x.arg().data()
    //                     , x.arg().size());
    //
    //                 if (!node_id_opt) {
    //                     LOGE(TAG, "Bad node identifier");
    //                     return EXIT_FAILURE;
    //                 }
    //             } else {
    //                 expectedArgError = true;
    //             }
    //         } else if (x.is_option("node-addr")) {
    //             if (x.has_arg()) {
    //                 auto addr = netty::inet4_addr::parse(x.arg());
    //
    //                 if (!addr) {
    //                     LOGE(TAG, "Bad address for '{}'", to_string(x.optname()));
    //                     return EXIT_FAILURE;
    //                 }
    //
    //                 neighbor_node_saddrs.push_back(netty::socket4_addr{*addr, PORT});
    //             } else {
    //                 expectedArgError = true;
    //             }
    //         } else {
    //             LOGE(TAG, "Bad arguments. Try --help option.");
    //             return EXIT_FAILURE;
    //         }
    //
    //         if (expectedArgError) {
    //             print_usage(programName, "Expected argument for " + to_string(x.optname()));
    //             return EXIT_FAILURE;
    //         }
    //     }
    // } else {
    //     print_usage(programName);
    //     return EXIT_SUCCESS;
    // }
    //
    // if (!node_id_opt) {
    //     LOGE(TAG, "Node identifier must be specified");
    //     return EXIT_FAILURE;
    // }
    //
    // if (neighbor_node_saddrs.empty()) {
    //     LOGE(TAG, "No neighbor node address specified");
    //     return EXIT_FAILURE;
    // }
    //
    // // For priority randoimization
    // std::srand(std::time({}));
    //
    // netty::startup_guard netty_startup;
    //
    // meshnet_node_t * node_ptr = nullptr;
    // meshnet_node_t::callback_suite callbacks;
    //
    // callbacks.on_node_connected = [& node_ptr] (meshnet_node_t::node_id id) {
    //     LOGD(TAG, "Node connected: {}", id);
    //     // node_ptr->set_frame_size(id, 16);
    //
    //     std::string msg0 = "Hello, meshnet node [priority=0]: " + to_string(id);
    //     std::string msg1 = "Hello, meshnet node [priority=1]: " + to_string(id);
    //     std::string msg2 = "Hello, meshnet node [priority=2]: " + to_string(id);
    //     node_ptr->send(id, 2, msg2.data(), msg2.size());
    //     node_ptr->send(id, 1, msg1.data(), msg1.size());
    //     node_ptr->send(id, 0, msg0.data(), msg0.size());
    // };
    //
    // callbacks.on_node_disconnected = [] (meshnet_node_t::node_id id) {
    //     LOGD(TAG, "Node disconnected: {}", id);
    // };
    //
    // callbacks.on_message_received = [& node_ptr] (meshnet_node_t::node_id id, std::vector<char> && bytes) {
    //     LOGD(TAG, "Message received from node: {}: {}", id, std::string(bytes.data(), bytes.size()));
    //
    //     int priority = std::rand() % priority_writer_queue_t::priority_count();
    //     int repetition_count = std::rand() % 1000;
    //     // int repetition_count = std::rand() % 10;
    //
    //     if (repetition_count == 0)
    //         repetition_count = 1;
    //
    //     std::string msg;
    //
    //     for (int i = 0; i < repetition_count; i++)
    //         msg += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    //
    //     node_ptr->send(id, priority, msg.data(), msg.size());
    // };
    //
    // meshnet_node_t node(*node_id_opt, false, std::move(callbacks));
    // node_ptr = & node;
    //
    // netty::inet4_addr listenerAddr = netty::inet4_addr{netty::inet4_addr::any_addr_value};
    // node.add_listener(netty::socket4_addr{listenerAddr, PORT});
    // node.listen();
    //
    // for (auto const & saddr: neighbor_node_saddrs)
    //     node.connect_host(saddr);
    //
    // while (!quit_flag.load())
    //     node.step(std::chrono::milliseconds {10});

    return EXIT_SUCCESS;
}
