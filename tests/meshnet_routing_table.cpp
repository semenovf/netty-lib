////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.11 Initial version.
////////////////////////////////////////////////////////////////////////////////
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "meshnode.hpp"
#include <pfs/universal_id_hash.hpp>
#include <pfs/synchronized.hpp>
#include <pfs/lorem/lorem_ipsum.hpp>
#include <pfs/netty/startup.hpp>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>

// Test scheme
// A0, A1, B0, B1, C0, C1, D0, D1 - regular nodes (nodes)
// a, b, c, d - gateway nodes (gateways)
//
//                     B0   B1
//                      |   |
//                      +---+
//                        |
//                 +----- b -----+
//   A0-----+      |             |      +-----C0
//          |----- a ----------- c -----|
//   A1-----+      |             |      +-----C1
//                 +----- d -----+
//                        |
//                      +---+
//                      |   |
//                     D0   D1
//

using namespace netty::patterns;

// Black        0;30     Dark Gray     1;30
// Blue         0;34     Light Blue    1;34
// Purple       0;35     Light Purple  1;35

#define COLOR(x) "\033[" #x "m"
#define LGRAY     COLOR(0;37)
#define GREEN     COLOR(0;32)
#define LGREEN    COLOR(1;32)
#define RED       COLOR(0;31)
#define LRED      COLOR(1;31)
#define CYAN      COLOR(0;36)
#define LCYAN     COLOR(1;36)
#define WHITE     COLOR(1;37)
#define ORANGE    COLOR(0;33)
#define YELLOW    COLOR(1;33)
#define END_COLOR COLOR(0)

static constexpr char const * TAG = CYAN "meshnet-test" END_COLOR;

static std::atomic_int channels_established_counter {0};
static std::atomic_int channels_destroyed_counter {0};

namespace common {
    struct item
    {
        std::shared_ptr<node_pool_t> node_pool;
        std::uint16_t port;
    };

    std::unordered_map<std::string, item> nodes;
    pfs::synchronized<std::unordered_map<node_pool_t::node_id, std::string>> name_dictionary;
    std::vector<std::thread> threads;

    void sigterm_handler (int sig)
    {
        MESSAGE("Force interrupt all nodes by signal: ", sig);

        for (auto & x: nodes)
            x.second.node_pool->interrupt();
    }

    inline void sleep (int timeout, std::string const & description = std::string{})
    {
        if (description.empty()) {
            LOGD(TAG, "Waiting for {} seconds", timeout);
        } else {
            LOGD(TAG, "{}: waiting for {} seconds", description, timeout);
        }

        std::this_thread::sleep_for(std::chrono::seconds{timeout});
    }

    std::shared_ptr<node_pool_t> get_node_pool (std::string const & name)
    {
        auto pos = nodes.find(name);

        if (pos != nodes.end())
            return pos->second.node_pool;

        return nullptr;
    }

    std::uint16_t get_port (std::string const & name)
    {
        auto pos = nodes.find(name);

        if (pos != nodes.end())
            return pos->second.port;

        return 0;
    }

    void interrupt (std::string const & name)
    {
        auto ptr = get_node_pool(name);

        if (ptr)
            ptr->interrupt();
    }

    void connect_host (meshnet::node_index_t index, std::string const & initiator_name
        , std::string const & target_name, bool behind_nat = false)
    {
        auto initiator = get_node_pool(initiator_name);
        auto target = get_node_pool(target_name);

        netty::socket4_addr target_saddr {netty::inet4_addr {127, 0, 0, 1}, get_port(target_name)};
        initiator->connect_host(index, target_saddr, behind_nat);
    }

    void connect_host (meshnet::node_index_t index, std::string const & initiator_name
        , netty::socket4_addr const & target_saddr, bool behind_nat = false)
    {
        auto initiator = get_node_pool(initiator_name);
        initiator->connect_host(index, target_saddr, behind_nat);
    }

    std::string node_name (node_pool_t::node_id const & id)
    {
        return name_dictionary.rlock()->at(id);
    }

    std::string node_name (node_pool_t::node_id_rep id_rep)
    {
        return node_name(node_pool_t::node_id_traits::cast(id_rep));
    }

    void run_all ()
    {
        for (auto & x: nodes) {
            auto ptr = x.second.node_pool;

            threads.emplace_back([ptr] () {
                ptr->run();
            });
        }
    }

    void interrupt_all ()
    {
        for (auto & x: nodes) {
            auto ptr = x.second.node_pool;
            ptr->interrupt();
        }
    }

    void join_all ()
    {
        for (auto & x: threads)
            x.join();
    }
}

static void create_node_pool (node_pool_t::node_id id, std::string name, std::uint16_t port, bool is_gateway)
{
    node_pool_t::options opts;
    opts.id = id;
    opts.name = name;
    opts.is_gateway = is_gateway;
    netty::socket4_addr listener_saddr {netty::inet4_addr {127, 0, 0, 1}, port};

    node_pool_t::callback_suite callbacks;

    callbacks.on_error = [] (std::string const & msg) {
        LOGE(TAG, "{}", msg);
    };

    callbacks.on_channel_established = [name] (node_t::node_id_rep id_rep, bool is_gateway) {
        auto node_type = is_gateway ? "gateway node" : "regular node";

        LOGD(TAG, "{}: Channel established with {}: {}", name, node_type, common::node_name(id_rep));
        ++channels_established_counter;
    };

    callbacks.on_channel_destroyed = [name] (node_t::node_id_rep id_rep) {
        LOGD(TAG, "{}: Channel destroyed with {}", name, common::node_name(id_rep));
        ++channels_destroyed_counter;
    };

    // Notify when node alive status changed
    callbacks.on_node_alive = [name] (node_t::node_id_rep id_rep) {
        LOGD(TAG, "{}: Node alive: {}", name, common::node_name(id_rep));
    };

    // Notify when node alive status changed
    callbacks.on_node_expired = [name] (node_t::node_id_rep id_rep) {
        LOGD(TAG, "{}: Node expired: {}", name, common::node_name(id_rep));
    };

    // Notify when node alive status changed
    callbacks.on_route_ready = [id, name] (node_t::node_id_rep dest, std::uint16_t hops) {
        if (hops == 0) {
            // Gateway is this node when direct access
            LOGD(TAG, "{}: " LGREEN "Route ready" END_COLOR ": {}->{} (" LGREEN "direct access" END_COLOR ")"
                , name
                , node_pool_t::node_id_traits::to_string(id)
                , node_pool_t::node_id_traits::to_string(dest));
        } else {
            LOGD(TAG, "{}: " LGREEN "Route ready" END_COLOR ": {}->{} (" LGREEN "hops={}" END_COLOR ")"
                , name
                , node_pool_t::node_id_traits::to_string(id)
                , node_pool_t::node_id_traits::to_string(dest)
                , hops);
        }
    };


    auto node_pool = std::make_shared<node_pool_t>(std::move(opts), std::move(callbacks));
    auto node_index = node_pool->add_node<node_t>({listener_saddr});

    node_pool->listen(node_index, 10);

    common::name_dictionary.wlock()->insert({node_pool->id(), name});
    common::nodes.insert({std::move(name), {node_pool, port}});
}

TEST_CASE("meshnet routing table") {
    netty::startup_guard netty_startup;
    bool behind_nat = true;

    // Create gateways
    create_node_pool("01JQN2NGY47H3R81Y9SG0F0A00"_uuid, "a", 4210, true);
    create_node_pool("01JQN2NGY47H3R81Y9SG0F0B00"_uuid, "b", 4220, true);

    // Create regular nodes
    create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false);
    // create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0_dup", 4201, false); // FIXME Uncomment later
    // create_node_pool("01JQC29M6RC2EVS1ZST11P0VA1"_uuid, "A1", 4212, false);
    create_node_pool("01JQC29M6RC2EVS1ZST11P0VB0"_uuid, "B0", 4221, false);
    // create_node_pool("01JQC29M6RC2EVS1ZST11P0VB1"_uuid, "B1", 4222, false);

    // Connect gateways
    common::connect_host(1, "a", "b");
    common::connect_host(1, "b", "a");

    common::connect_host(1, "A0", "a", behind_nat);
    // common::connect_host(1, "A1", "a", behind_nat);

    common::connect_host(1, "B0", "b", behind_nat);
    // common::connect_host(1, "B1", "b", behind_nat);


    // common::connect_host(1, "A0", "A0_dup"); // FIXME Uncomment later
    // common::connect_host(1, "A0", "A1");
    // common::connect_host(1, "A1", "A0");
    // common::connect_host(1, "B0", "B1");
    // common::connect_host(1, "B1", "B0");


    signal(SIGINT, common::sigterm_handler);

    common::run_all();

    common::sleep(1, "Check channels established");
    // CHECK_EQ(channels_established_counter.load(), 10);

    // common::interrupt_all();

    common::join_all();
}
