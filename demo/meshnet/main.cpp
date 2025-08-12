////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "transport.hpp"
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
#include <map>
#include <queue>
#include <set>
#include <utility>
#include <signal.h>

namespace fs = pfs::filesystem;
using string_view = pfs::string_view;
using pfs::to_string;

static std::atomic_bool s_quit_flag {false};
static bool s_sending_in_loop {false};

class file_tracker
{
private:
    std::queue<std::pair<int, fs::path>> _q;
    bool _looping {false};
    bool _initial_send {true};

private: // static
    static std::vector<std::pair<int, fs::path>> _s_common_files;
    static std::map<node_id, std::vector<std::pair<int, fs::path>>> _s_files;

    static std::map<node_id, file_tracker> _s_trackers;

private:

public: // static
    static void add (int priority, fs::path path)
    {
        _s_common_files.emplace_back(priority, std::move(path));
    }

    static void add (node_id id, int priority, fs::path path)
    {
        _s_files[id].emplace_back(priority, std::move(path));
    }

    static void prepare_tracker_for (node_id id, bool looping)
    {
        auto & t = _s_trackers[id];
        t._looping = looping;
        t._initial_send = true;

        std::queue<std::pair<int, fs::path>> empty;
        t._q.swap(empty);

        for (auto const & x: _s_common_files)
            t._q.push(x);

        for (auto const & x: _s_files[id])
            t._q.push(x);
    }

    template <typename F>
    static void send_to (node_id id, F && send_cb)
    {
        auto pos = _s_trackers.find(id);

        if (pos == _s_trackers.end())
            return;

        auto & t = pos->second;
        auto & q = t._q;

        if (q.empty())
            return;

        std::size_t counter = t._initial_send ? q.size() : 1;

        while (counter-- > 0) {
            auto x = q.front();
            q.pop();

            // file path------------+
            // priority----+        |
            //             v        v
            send_cb(id, x.first, x.second);

            if (t._looping)
                q.push(x);
        }

        t._initial_send = false;
    }
};

std::vector<std::pair<int, fs::path>> file_tracker::_s_common_files;
std::map<node_id, std::vector<std::pair<int, fs::path>>> file_tracker::_s_files;
std::map<node_id, file_tracker> file_tracker::_s_trackers;

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

static void print_usage (fs::path const & programName
    , std::string const & errorString = std::string{})
{
    if (!errorString.empty())
        LOGE(TAG, "{}", errorString);

    fmt::println("Usage:\n\n"
        "{0} --help | -h\n"
        "{0} [--reliable] [--id=NODE_ID] [--gw]\n"
        "\t\t{{--node --port=PORT... --nb[-nat]=ADDR:PORT...}}... [--loop] {{[--priority=PRIOR] [--send=PATH...] [--send-to=NODE_ID@PATH...]}}...\n\n"

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
        "--loop\n"
        "\tSending common files in an infinite loop\n\n"
        "--priority=PRIOR\n"
        "\tThe priority with which subsequent files should be sent\n\n"
        "--send=PATH\n"
        "\tSend file to all nodes when first alive event occurred\n\n"
        "--send-to=NODE_ID@PATH\n"
        "\tSend file to specified node when first alive event occurred\n\n"

        "Examples:\n\n"
        "Run with connection to 192.168.0.2:\n"
        "\t{0} --name=\"Node Name\" --id=01JW83N29KV04QNATTK82Z5NTX --node --port=4242 --nb=192.168.0.2:4242\n"
        , programName);
}

// Check node specilizations
static void dumb ()
{
    auto id = pfs::generate_uuid();
    bool is_gateway = false;

    bare_meshnet_node_t {id, is_gateway};
    nopriority_meshnet_node_t {id, is_gateway};
    priority_meshnet_node_t {id, is_gateway};
}

static void send_file (reliable_node_pool_t & node_pool, node_t::node_id id, int priority
    , fs::path const & file_to_send)
{
    if (!file_to_send.empty()) {
        auto file = ionik::local_file::open_read_only(file_to_send);

        if (file) {
            auto msgid = pfs::generate_uuid();
            auto data = file.read_all();
            LOGD(TAG, "Send file: {}", file_to_send);
            node_pool.enqueue_message(id, msgid, priority, data.data(), data.size());
        }
    }
}

template <typename NodePool>
void configure_node (NodePool &);

template <>
void configure_node<node_pool_t> (node_pool_t &)
{}

template <>
void configure_node<reliable_node_pool_t> (reliable_node_pool_t & node_pool)
{
    node_pool.on_node_alive([& node_pool] (node_t::node_id id)
    {
#ifndef NETTY__TRACE_ENABLED
        LOGD(TAG, "Node alive: {}", to_string(id));
#endif
        file_tracker::prepare_tracker_for(id, s_sending_in_loop);
        file_tracker::send_to(id, [& node_pool] (node_t::node_id id, int priority, fs::path const & path) {
            send_file(node_pool, id, priority, path);
        });
    });

    node_pool.on_receiver_ready([& node_pool] (node_t::node_id id)
    {
#ifndef NETTY__TRACE_ENABLED
        LOGD(TAG, "Receiver ready: {}", to_string(id));
#endif
    });

    node_pool.on_message_received([] (node_id id, message_id msgid, int priority, std::vector<char> msg)
    {
#ifndef NETTY__TRACE_ENABLED
        fmt::println("");
        LOGD(TAG, "Message received from: {}: msgid={}, priority={}, size={}", to_string(id)
            , to_string(msgid), priority, msg.size());
#endif
    });

    node_pool.on_message_delivered([& node_pool] (node_id id, message_id msgid)
    {
#ifndef NETTY__TRACE_ENABLED
        LOGD(TAG, "Message delivered to: {}: msgid={}", to_string(id), to_string(msgid));
#endif
        file_tracker::send_to(id, [& node_pool] (node_t::node_id id, int priority, fs::path const & path) {
            send_file(node_pool, id, priority, path);
        });
    });

    node_pool.on_message_lost([& node_pool] (node_id id, message_id msgid)
    {
#ifndef NETTY__TRACE_ENABLED
        LOGD(TAG, "Message lost from: {}: msgid={}", to_string(id), to_string(msgid));
#endif
    });

    node_pool.on_message_receiving_begin([] (node_id id, message_id msgid, std::size_t total_size)
    {
#ifndef NETTY__TRACE_ENABLED
        LOGD(TAG, "Begin message receiving from: {}: msgid={}, size={}", to_string(id)
            , to_string(msgid), total_size);
#endif
    });

    node_pool.on_message_receiving_progress([] (node_id id, message_id msgid, std::size_t received_size
        , std::size_t total_size)
    {
        fmt::print("{}: {: 3} % ({}/{})\r", msgid
            , static_cast<int>(100 * (static_cast<double>(received_size)/total_size))
            , received_size, total_size);
    });
}

template <typename NodePool>
void run (NodePool & node_pool, std::vector<node_item> & nodes)
{
    node_pool.on_channel_established([& node_pool] (node_t::node_id id, bool is_gateway) {
        auto node_type = is_gateway ? "gateway node" : "regular node";

#ifndef NETTY__TRACE_ENABLED
        LOGD(TAG, "Channel established with {}: {}", node_type, to_string(id));
#endif
    });

    node_pool.on_channel_destroyed([] (node_t::node_id id) {
#ifndef NETTY__TRACE_ENABLED
        LOGD(TAG, "Channel destroyed with {}", to_string(id));
#endif
    });

    // Notify when node alive status changed
    node_pool.on_node_alive([] (node_t::node_id id) {
#ifndef NETTY__TRACE_ENABLED
        LOGD(TAG, "Node alive: {}", to_string(id));
#endif
    });

    // Notify when node alive status changed
    node_pool.on_node_expired([] (node_t::node_id id) {
#ifndef NETTY__TRACE_ENABLED
        LOGD(TAG, "Node expired: {}", to_string(id));
#endif
    });

    configure_node(node_pool);

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
    int current_priority = 0;

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
            } else if (x.is_option("loop")) {
                s_sending_in_loop = true;
            } else if (x.is_option("priority")) {
                if (x.has_arg()) {
                    std::error_code ec;
                    current_priority = pfs::to_integer<int>(x.arg().begin(), x.arg().end(), 0
                        , static_cast<int>(priority_tracker_t::SIZE) - 1, ec);

                    if (ec) {
                        LOGE(TAG, "Bad priority");
                        return EXIT_FAILURE;
                    }
                } else {
                    expectedArgError = true;
                }
            } else if (x.is_option("send")) {
                if (x.has_arg()) {
                    auto path = pfs::utf8_decode_path(to_string(x.arg()));

                    if (!fs::is_regular_file(path)) {
                        LOGE(TAG, "Expected regular file to send: {}", x.arg());
                        return EXIT_FAILURE;
                    }

                    file_tracker::add(current_priority, std::move(path));
                } else {
                    expectedArgError = true;
                }
            } else if (x.is_option("send-to")) {
                if (x.has_arg()) {
                    // size_t start = 0;
                    size_t delim_pos = x.arg().find('@');

                    if (delim_pos == pfs::string_view::npos) {
                        LOGE(TAG, "Bad send-to argument: {}", x.arg());
                        return EXIT_FAILURE;
                    }

                    auto id_sv = x.arg().substr(0, delim_pos);
                    delim_pos++;
                    auto path_sv = x.arg().substr(delim_pos);

                    auto id_opt = pfs::parse_universal_id(id_sv.data(), id_sv.size());

                    if (!id_opt) {
                        LOGE(TAG, "Bad node identifier");
                        return EXIT_FAILURE;
                    }

                    auto path = pfs::utf8_decode_path(to_string(path_sv));

                    if (!fs::is_regular_file(path)) {
                        LOGE(TAG, "Expected regular file to send: {}", x.arg());
                        return EXIT_FAILURE;
                    }

                    file_tracker::add(*id_opt, current_priority, std::move(path));
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
        run(node_pool, nodes);
    } else {
        node_pool_t node_pool {id, is_gateway};
        run(node_pool, nodes);
    }

    return EXIT_SUCCESS;
}
