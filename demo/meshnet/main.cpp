////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "patterns/transport.hpp"
#include "tag.hpp"
#include <pfs/argvapi.hpp>
#include <pfs/countdown_timer.hpp>
#include <pfs/filesystem.hpp>
#include <pfs/fmt.hpp>
#include <pfs/integer.hpp>
#include <pfs/log.hpp>
#include <pfs/standard_paths.hpp>
#include <pfs/string_view.hpp>
#include <pfs/ionik/local_file.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <pfs/netty/startup.hpp>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <signal.h>

namespace fs = pfs::filesystem;
using string_view = pfs::string_view;
using pfs::to_string;

static std::atomic_bool s_quit_flag {false};
static std::atomic_bool s_already_sent {false};

static void sigterm_handler (int /*sig*/)
{
    s_quit_flag.store(true);
}

struct node_item
{
    bool behind_nat {false};
    std::vector<netty::socket4_addr> listener_saddrs;
    std::vector<std::pair<netty::socket4_addr, bool>> neighbor_saddrs;
};

static void print_usage (pfs::filesystem::path const & programName
    , std::string const & errorString = std::string{})
{
    if (!errorString.empty())
        LOGE(TAG, "{}", errorString);

    fmt::println("Usage:\n\n"
        "{0} --help | -h\n"
        "{0} [--reliable] [--id=NODE_ID] [--gw]\n"
        "\t\t{{--node --port=PORT... --nb[-nat]=ADDR:PORT...}}... [--send=PATH...]\n\n"

        "Options:\n\n"
        "--help | -h\n"
        "\tPrint this help and exit\n"
        "--reliable\n"
        "\tUse reliable implementation of node pool\n"
        "--id=NODE_ID\n"
        "\tThis node identifier\n"
        "--gw\n"
        "\tThis node is a gateway\n\n"

        "--node\n"
        "\tStart node parameters\n\n"
        "--port=PORT...\n"
        "\tRun listeners for node on specified ports\n\n"
        "--nb=ADDR:PORT...\n"
        "--nb-nat=ADDR:PORT...\n"
        "\tNeighbor nodes addresses. --nb-nat specifies the node behind NAT\n\n"

        "Examples:\n\n"
        "Run with connection to 192.168.0.2:\n"
        "\t{0} --name=\"Node Name\" --id=01JW83N29KV04QNATTK82Z5NTX --node --port=4242 --nb=192.168.0.2:4242\n"
        , programName);
}

// Check node specilizations
void dumb ()
{
    auto id = pfs::generate_uuid();
    bool is_gateway = false;

    bare_meshnet_node_t {id, is_gateway};
    nopriority_meshnet_node_t {id, is_gateway};
    priority_meshnet_node_t {id, is_gateway};
}

void send_file (reliable_node_pool_t & node_pool, node_t::node_id id, pfs::filesystem::path file_to_send)
{
    if (!file_to_send.empty()) {
        auto file = ionik::local_file::open_read_only(file_to_send);

        if (file) {
            auto msgid = pfs::generate_uuid();
            auto data = file.read_all();
            int priority = 1;
            LOGD(TAG, "Send file: {}", file_to_send);
            node_pool.enqueue_message(id, msgid, priority, false, data.data(), data.size());
        }
    }
}

template <typename NodePool>
void configure_node (NodePool &, std::vector<fs::path>);

template <>
void configure_node<node_pool_t> (node_pool_t &, std::vector<fs::path>)
{}

template <>
void configure_node<reliable_node_pool_t> (reliable_node_pool_t & node_pool
    , std::vector<fs::path> files_to_send)
{
    node_pool.on_node_alive([& node_pool, files_to_send = std::move(files_to_send)] (node_t::node_id id)
    {
        LOGD(TAG, "Node alive: {}", to_string(id));

        if (!s_already_sent) {
            for (auto const & file_path: files_to_send)
                send_file(node_pool, id, file_path);

            s_already_sent = true;
        }
    });

    node_pool.on_receiver_ready([& node_pool] (node_t::node_id id)
    {
        LOGD(TAG, "Receiver ready: {}", to_string(id));
    });

    node_pool.on_message_received([] (node_id id, message_id msgid
        , int priority, std::vector<char> msg)
    {
        fmt::println("");
        LOGD(TAG, "Message received from: {}: msgid={}, priority={}, size={}", to_string(id)
            , to_string(msgid), priority, msg.size());
    });

    node_pool.on_message_delivered([] (node_id id, message_id msgid)
    {
        LOGD(TAG, "Message delivered to: {}: msgid={}", to_string(id), to_string(msgid));
    });

    node_pool.on_message_receiving_begin([] (node_id id, message_id msgid, std::size_t total_size)
    {
        LOGD(TAG, "Begin message receiving from: {}: msgid={}, size={}", to_string(id)
            , to_string(msgid), total_size);
    });

    node_pool.on_message_receiving_progress([] (node_id id, message_id msgid, std::size_t received_size
        , std::size_t total_size)
    {
        fmt::print("{} %\r", static_cast<int>(100 * (static_cast<double>(received_size)/total_size)));
    });
}

template <typename NodePool>
void run (NodePool & node_pool, std::vector<node_item> & nodes, std::vector<fs::path> files_to_send)
{
    node_pool.on_channel_established([& node_pool] (node_t::node_id id, bool is_gateway) {
        auto node_type = is_gateway ? "gateway node" : "regular node";
        LOGD(TAG, "Channel established with {}: {}", node_type, to_string(id));
    });

    node_pool.on_channel_destroyed([] (node_t::node_id id) {
        LOGD(TAG, "Channel destroyed with {}", to_string(id));
    });

    // Notify when node alive status changed
    node_pool.on_node_alive([] (node_t::node_id id) {
        LOGD(TAG, "Node alive: {}", to_string(id));
    });

    // Notify when node alive status changed
    node_pool.on_node_expired([] (node_t::node_id id) {
        LOGD(TAG, "Node expired: {}", to_string(id));
    });

    configure_node(node_pool, std::move(files_to_send));

    for (auto & item: nodes) {
        auto node_index = node_pool.template add_node<node_t>(item.listener_saddrs);
        node_pool.listen(node_index, 10);

        for (auto const & x: item.neighbor_saddrs)
            node_pool.connect_host(node_index, x.first, x.second);
    }

    while (!s_quit_flag.load()) {
        pfs::countdown_timer<std::milli> countdown_timer {std::chrono::milliseconds {10}};
        auto n = node_pool.step();

        if (n == 0)
            std::this_thread::sleep_for(countdown_timer.remain());
    }
}

int main (int argc, char * argv[])
{
    signal(SIGINT, sigterm_handler);
    signal(SIGTERM, sigterm_handler);

    bool is_reliable_impl = false;
    auto id = pfs::generate_uuid();
    bool is_gateway = false;
    std::vector<node_item> nodes;
    std::vector<fs::path> files_to_send;

    auto commandLine = pfs::make_argvapi(argc, argv);
    auto programName = commandLine.program_name();
    auto commandLineIterator = commandLine.begin();

    if (commandLineIterator.has_more()) {
        while (commandLineIterator.has_more()) {
            auto x = commandLineIterator.next();
            auto expectedArgError = false;
            auto expectedNodeOption = false;

            if (x.is_option("help") || x.is_option("h")) {
                print_usage(programName);
                return EXIT_SUCCESS;
            } else if (x.is_option("reliable")) {
                is_reliable_impl = true;
            } else if (x.is_option("id")) {
                if (x.has_arg()) {
                    auto id_opt = pfs::parse_universal_id(x.arg().data(), x.arg().size());

                    if (id_opt) {
                        id = *id_opt;
                    } else {
                        LOGE(TAG, "Bad node identifier");
                        return EXIT_FAILURE;
                    }
                } else {
                    expectedArgError = true;
                }
            } else if (x.is_option("gw")) {
                is_gateway = true;
            } else if (x.is_option("node")) {
                nodes.emplace_back(); // Emplace back empty item
            } else if (x.is_option("port")) {
                if (!nodes.empty()) {
                    if (x.has_arg()) {
                        std::error_code ec;
                        auto port = pfs::to_integer<std::uint16_t>(x.arg().begin(), x.arg().end()
                            , std::uint16_t{1024}, std::uint16_t{65535}, ec);

                        if (ec) {
                            LOGE(TAG, "Bad port");
                            return EXIT_FAILURE;
                        }

                        netty::socket4_addr listener {netty::inet4_addr {netty::inet4_addr::any_addr_value}, port};
                        nodes.back().listener_saddrs.push_back(std::move(listener));
                    } else {
                        expectedArgError = true;
                    }
                } else {
                    expectedNodeOption = true;
                }
            } else if (x.is_option("nb") || x.is_option("nb-nat")) {
                if (!nodes.empty()) {
                    bool nat = x.is_option("nb-nat");

                    if (x.has_arg()) {
                        auto saddr_opt = netty::socket4_addr::parse(x.arg());

                        if (!saddr_opt) {
                            LOGE(TAG, "Bad socket address for '{}'", to_string(x.optname()));
                            return EXIT_FAILURE;
                        }

                        nodes.back().neighbor_saddrs.push_back(std::make_pair(*saddr_opt, nat));
                    } else {
                        expectedArgError = true;
                    }
                } else {
                    expectedNodeOption = true;
                }
            } else if (x.is_option("send")) {
                if (x.has_arg()) {
                    auto path = pfs::utf8_decode_path(to_string(x.arg()));

                    if (!fs::is_regular_file(path)) {
                        LOGE(TAG, "Expected regular file to send: {}", x.arg());
                        return EXIT_FAILURE;
                    }

                    files_to_send.push_back(std::move(path));
                } else {
                    expectedArgError = true;
                }
            } else {
                LOGE(TAG, "Bad arguments. Try --help option.");
                return EXIT_FAILURE;
            }

            if (expectedNodeOption) {
                print_usage(programName, "Expected --node option before " + to_string(x.optname()));
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

    if (nodes.empty()) {
        LOGE(TAG, "No nodes");
        return EXIT_FAILURE;
    }

    netty::startup_guard netty_startup;

    if (is_reliable_impl) {
        reliable_node_pool_t node_pool {id, is_gateway};
        run(node_pool, nodes, std::move(files_to_send));
    } else {
        node_pool_t node_pool {id, is_gateway};
        run(node_pool, nodes, std::move(files_to_send));
    }

    return EXIT_SUCCESS;
}
