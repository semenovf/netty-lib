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
#include "boolean_matrix2.hpp"
#include "meshnode.hpp"
#include <pfs/assert.hpp>
#include <pfs/fmt.hpp>
#include <pfs/synchronized.hpp>
#include <pfs/universal_id_hash.hpp>
#include <pfs/lorem/lorem_ipsum.hpp>
#include <pfs/netty/startup.hpp>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>

// =================================================================================================
// Legend
// -------------------------------------------------------------------------------------------------
// A0, A1, B0, B1, C0, C1, D0, D1 - regular nodes (nodes)
// a, b, c, d - gateway nodes (gateways)
//
// =================================================================================================
// Test scheme 1
// -------------------------------------------------------------------------------------------------
//  A0----- a -----B0
//
// =================================================================================================
// Test scheme 2
// -------------------------------------------------------------------------------------------------
//   A0-----+             +-----B0
//          |----- a -----|
//   A1-----+             +-----B1
//
// =================================================================================================
// Test scheme 3
// -------------------------------------------------------------------------------------------------
//  A0----- a ----------- b -----B0
//
// =================================================================================================
// Test scheme 4 (not implemented yet)
// -------------------------------------------------------------------------------------------------
//   A0-----+                           +-----B0
//          |----- a ----------- b -----|
//   A1-----+                           +-----B1
//
// =================================================================================================
// Test scheme 5 (not implemented yet)
// -------------------------------------------------------------------------------------------------
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
#define ITERATION_COUNT 1 //10

#define TEST_SCHEME_1_ENABLED 0
#define TEST_SCHEME_2_ENABLED 0
#define TEST_SCHEME_3_ENABLED 0
#define TEST_SCHEME_4_ENABLED 0
#define TEST_SCHEME_5_ENABLED 1

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

static pfs::synchronized<boolean_matrix2<3>> s_route_matrix_1;
static pfs::synchronized<boolean_matrix2<5>> s_route_matrix_2;
static pfs::synchronized<boolean_matrix2<4>> s_route_matrix_3;
static pfs::synchronized<boolean_matrix2<6>> s_route_matrix_4;
static pfs::synchronized<boolean_matrix2<12>> s_route_matrix_5;

// See https://github.com/doctest/doctest/issues/345
inline char const * current_doctest_name ()
{
    return doctest::detail::g_cs->currentTest->m_name;
}

#define START_TEST_MESSAGE MESSAGE("START Test: ", std::string(current_doctest_name()));
#define END_TEST_MESSAGE MESSAGE("END Test: ", std::string(current_doctest_name()));

namespace tools {
    struct context
    {
        std::shared_ptr<node_pool_t> node_pool;
        std::uint16_t port;
        std::size_t serial_number; // Serial number used in matrix to check tests results
    };

    pfs::synchronized<std::unordered_map<node_pool_t::node_id, std::string>> name_dictionary;
    std::unordered_map<std::string, context> nodes;
    std::vector<std::thread> threads;
    static std::atomic_int channels_established_counter {0};
    static std::atomic_int channels_destroyed_counter {0};

    void clear ()
    {
        name_dictionary.wlock()->clear();
        nodes.clear();
        threads.clear();
        channels_established_counter = 0;
        channels_destroyed_counter = 0;
        s_route_matrix_1.wlock()->reset();
        s_route_matrix_2.wlock()->reset();
        s_route_matrix_3.wlock()->reset();
        s_route_matrix_4.wlock()->reset();
        s_route_matrix_5.wlock()->reset();
    }

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

    std::string node_name (node_pool_t::node_id const & id)
    {
        return name_dictionary.rlock()->at(id);
    }

    std::string node_name (node_pool_t::node_id_rep id_rep)
    {
        return node_name(node_pool_t::node_id_traits::cast(id_rep));
    }

    context * get_context (std::string const & name)
    {
        auto pos = nodes.find(name);

        if (pos != nodes.end())
            return & pos->second;

        PFS__TERMINATE(false, fmt::format("context not found by name: {}", name).c_str());
        return nullptr;
    }

    context * get_context (node_pool_t::node_id_rep id_rep)
    {
        return get_context(node_name(id_rep));
    }

    context * get_context (node_pool_t::node_id id)
    {
        return get_context(node_name(id));
    }

    void connect_host (meshnet::node_index_t index, std::string const & initiator_name
        , std::string const & target_name, bool behind_nat = false)
    {
        auto initiator_ctx = get_context(initiator_name);
        auto target_ctx    = get_context(target_name);

        netty::socket4_addr target_saddr {netty::inet4_addr {127, 0, 0, 1}, target_ctx->port};
        initiator_ctx->node_pool->connect_host(index, target_saddr, behind_nat);
    }

    void connect_host (meshnet::node_index_t index, std::string const & initiator_name
        , netty::socket4_addr const & target_saddr, bool behind_nat = false)
    {
        auto initiator_ctx = get_context(initiator_name);
        initiator_ctx->node_pool->connect_host(index, target_saddr, behind_nat);
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

    void interrupt (std::string const & name)
    {
        auto pctx = get_context(name);
        pctx->node_pool->interrupt();
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

    template <typename RouteMatrix>
    void print_matrix2 (RouteMatrix & m, std::vector<char const *> caption)
    {
        fmt::print("[   ]");

        for (std::size_t j = 0; j < m.columns(); j++)
            fmt::print("[{:^3}]", caption[j]);

        fmt::println("");

        for (std::size_t i = 0; i < m.rows(); i++) {
            fmt::print("[{:^3}]", caption[i]);

            for (std::size_t j = 0; j < m.columns(); j++) {
                if (i == j)
                    fmt::print("[XXX]");
                else if (m.test(i, j))
                    fmt::print("[{:^3}]", '+');
                else
                    fmt::print("[   ]");
            }

            fmt::println("");
        }
    }

} // namespace tools

template <typename RouteMatrix>
static void create_node_pool (node_pool_t::node_id id, std::string name, std::uint16_t port
    , bool is_gateway, std::size_t serial_number, RouteMatrix & route_matrix)
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

        LOGD(TAG, "{}: Channel established with {}: {}", name, node_type, tools::node_name(id_rep));
        ++tools::channels_established_counter;
    };

    callbacks.on_channel_destroyed = [name] (node_t::node_id_rep id_rep) {
        LOGD(TAG, "{}: Channel destroyed with {}", name, tools::node_name(id_rep));
        ++tools::channels_destroyed_counter;
    };

    // Notify when node alive status changed
    callbacks.on_node_alive = [name] (node_t::node_id_rep id_rep) {
        LOGD(TAG, "{}: Node alive: {}", name, tools::node_name(id_rep));
    };

    // Notify when node alive status changed
    callbacks.on_node_expired = [name] (node_t::node_id_rep id_rep) {
        LOGD(TAG, "{}: Node expired: {}", name, tools::node_name(id_rep));
    };

    // Notify when node alive status changed
    callbacks.on_route_ready = [id, name, & route_matrix] (node_t::node_id_rep dest, std::uint16_t hops) {
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

        auto this_ctx = tools::get_context(id);
        auto dest_ctx = tools::get_context(dest);

        auto row = this_ctx->serial_number;
        auto col = dest_ctx->serial_number;

        route_matrix.wlock()->set(row, col, true);
    };

    auto node_pool = std::make_shared<node_pool_t>(std::move(opts), std::move(callbacks));
    auto node_index = node_pool->add_node<node_t>({listener_saddr});

    node_pool->listen(node_index, 10);

    tools::name_dictionary.wlock()->insert({node_pool->id(), name});
    tools::nodes.insert({std::move(name), {node_pool, port, serial_number}});
}

// TEST_CASE("duplication") {
//     START_TEST_MESSAGE
//
//     netty::startup_guard netty_startup;
//
//     create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false, 0);
//     create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0_dup", 4201, false, 1);
//
//     tools::connect_host(1, "A0", "A0_dup");
//
//     tools::run_all();
//     tools::sleep(1, "Check channels established");
//     tools::interrupt_all();
//     tools::join_all();
//     tools::clear();
//
//     END_TEST_MESSAGE
// }

#if TEST_SCHEME_1_ENABLED
TEST_CASE("scheme 1") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        netty::startup_guard netty_startup;
        bool behind_nat = true;

        std::size_t serial_number = 0;

        // Create gateways
        create_node_pool("01JQN2NGY47H3R81Y9SG0F0A00"_uuid, "a", 4210, true, serial_number++, s_route_matrix_1);

        // Create regular nodes
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false, serial_number++, s_route_matrix_1);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VB0"_uuid, "B0", 4221, false, serial_number++, s_route_matrix_1);

        REQUIRE_EQ(serial_number, s_route_matrix_1.rlock()->rows());

        tools::connect_host(1, "A0", "a", behind_nat);
        tools::connect_host(1, "B0", "a", behind_nat);

        signal(SIGINT, tools::sigterm_handler);

        tools::run_all();
        tools::sleep(1, "Check channels established");
        tools::interrupt_all();
        tools::join_all();

        tools::print_matrix2(*s_route_matrix_1.rlock(), {"a", "A0", "B0"});

        REQUIRE_EQ(tools::channels_established_counter.load(), 4);
        REQUIRE_EQ(s_route_matrix_1.rlock()->count(), 6);

        tools::clear();

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_2_ENABLED
TEST_CASE("scheme 2") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        netty::startup_guard netty_startup;
        bool behind_nat = true;

        std::size_t serial_number = 0;

        // Create gateways
        create_node_pool("01JQN2NGY47H3R81Y9SG0F0A00"_uuid, "a", 4210, true, serial_number++, s_route_matrix_2);

        // Create regular nodes
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false, serial_number++, s_route_matrix_2);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VA1"_uuid, "A1", 4212, false, serial_number++, s_route_matrix_2);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VB0"_uuid, "B0", 4221, false, serial_number++, s_route_matrix_2);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VB1"_uuid, "B1", 4222, false, serial_number++, s_route_matrix_2);

        REQUIRE_EQ(serial_number, s_route_matrix_2.rlock()->rows());

        tools::connect_host(1, "A0", "a", behind_nat);
        tools::connect_host(1, "A1", "a", behind_nat);

        tools::connect_host(1, "B0", "a", behind_nat);
        tools::connect_host(1, "B1", "a", behind_nat);

        tools::connect_host(1, "A0", "A1");
        tools::connect_host(1, "A1", "A0");
        tools::connect_host(1, "B0", "B1");
        tools::connect_host(1, "B1", "B0");

        signal(SIGINT, tools::sigterm_handler);

        tools::run_all();
        tools::sleep(1, "Check channels established");
        tools::interrupt_all();
        tools::join_all();

        tools::print_matrix2(*s_route_matrix_2.rlock(), {"a", "A0", "A1", "B0", "B1" });

        REQUIRE_EQ(tools::channels_established_counter.load(), 12);
        REQUIRE_EQ(s_route_matrix_2.rlock()->count(), 30);

        tools::clear();

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_3_ENABLED
TEST_CASE("scheme 3") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        netty::startup_guard netty_startup;
        bool behind_nat = true;

        std::size_t serial_number = 0;

        // Create gateways
        create_node_pool("01JQN2NGY47H3R81Y9SG0F0A00"_uuid, "a", 4210, true, serial_number++, s_route_matrix_3);
        create_node_pool("01JQN2NGY47H3R81Y9SG0F0B00"_uuid, "b", 4220, true, serial_number++, s_route_matrix_3);

        // Create regular nodes
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false, serial_number++, s_route_matrix_3);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VB0"_uuid, "B0", 4221, false, serial_number++, s_route_matrix_3);

        REQUIRE_EQ(serial_number, s_route_matrix_3.rlock()->rows());

        // Connect gateways
        tools::connect_host(1, "a", "b");
        tools::connect_host(1, "b", "a");

        tools::connect_host(1, "A0", "a", behind_nat);
        tools::connect_host(1, "B0", "b", behind_nat);

        signal(SIGINT, tools::sigterm_handler);

        tools::run_all();
        tools::sleep(1, "Check channels established");
        tools::interrupt_all();
        tools::join_all();

        tools::print_matrix2(*s_route_matrix_3.rlock(), {"a", "b", "A0", "B0"});

        REQUIRE_EQ(tools::channels_established_counter.load(), 6);
        REQUIRE_EQ(s_route_matrix_3.rlock()->count(), 12);

        tools::clear();

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_4_ENABLED
TEST_CASE("scheme 4") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        netty::startup_guard netty_startup;
        bool behind_nat = true;

        std::size_t serial_number = 0;

        // Create gateways
        create_node_pool("01JQN2NGY47H3R81Y9SG0F0A00"_uuid, "a", 4210, true, serial_number++, s_route_matrix_4);
        create_node_pool("01JQN2NGY47H3R81Y9SG0F0B00"_uuid, "b", 4220, true, serial_number++, s_route_matrix_4);

        // Create regular nodes
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false, serial_number++, s_route_matrix_4);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VA1"_uuid, "A1", 4212, false, serial_number++, s_route_matrix_4);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VB0"_uuid, "B0", 4221, false, serial_number++, s_route_matrix_4);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VB1"_uuid, "B1", 4222, false, serial_number++, s_route_matrix_4);

        REQUIRE_EQ(serial_number, s_route_matrix_4.rlock()->rows());

        // Connect gateways
        tools::connect_host(1, "a", "b");
        tools::connect_host(1, "b", "a");

        tools::connect_host(1, "A0", "a", behind_nat);
        tools::connect_host(1, "A1", "a", behind_nat);

        tools::connect_host(1, "B0", "b", behind_nat);
        tools::connect_host(1, "B1", "b", behind_nat);

        tools::connect_host(1, "A0", "A1");
        tools::connect_host(1, "A1", "A0");
        tools::connect_host(1, "B0", "B1");
        tools::connect_host(1, "B1", "B0");

        signal(SIGINT, tools::sigterm_handler);

        tools::run_all();
        tools::sleep(1, "Check channels established");
        tools::interrupt_all();
        tools::join_all();

        tools::print_matrix2(*s_route_matrix_4.rlock(), {"a", "b", "A0", "A1", "B0", "B1"});

        REQUIRE_EQ(tools::channels_established_counter.load(), 14);
        REQUIRE_EQ(s_route_matrix_4.rlock()->count(), 30);

        tools::clear();

        END_TEST_MESSAGE
    }
}
#endif

#if TEST_SCHEME_5_ENABLED
TEST_CASE("scheme 5") {
    int iteration_count = ITERATION_COUNT;

    while (iteration_count-- > 0) {
        START_TEST_MESSAGE

        netty::startup_guard netty_startup;
        bool behind_nat = true;

        std::size_t serial_number = 0;

        // Create gateways
        create_node_pool("01JQN2NGY47H3R81Y9SG0F0A00"_uuid, "a", 4210, true, serial_number++, s_route_matrix_5);
        create_node_pool("01JQN2NGY47H3R81Y9SG0F0B00"_uuid, "b", 4220, true, serial_number++, s_route_matrix_5);
        create_node_pool("01JQN2NGY47H3R81Y9SG0F0C00"_uuid, "c", 4230, true, serial_number++, s_route_matrix_5);
        create_node_pool("01JQN2NGY47H3R81Y9SG0F0D00"_uuid, "d", 4240, true, serial_number++, s_route_matrix_5);

        // Create regular nodes
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", 4211, false, serial_number++, s_route_matrix_5);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VA1"_uuid, "A1", 4212, false, serial_number++, s_route_matrix_5);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VB0"_uuid, "B0", 4221, false, serial_number++, s_route_matrix_5);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VB1"_uuid, "B1", 4222, false, serial_number++, s_route_matrix_5);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VC0"_uuid, "C0", 4231, false, serial_number++, s_route_matrix_5);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VC1"_uuid, "C1", 4232, false, serial_number++, s_route_matrix_5);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VD0"_uuid, "D0", 4241, false, serial_number++, s_route_matrix_5);
        create_node_pool("01JQC29M6RC2EVS1ZST11P0VD1"_uuid, "D1", 4242, false, serial_number++, s_route_matrix_5);

        REQUIRE_EQ(serial_number, s_route_matrix_5.rlock()->rows());

        // Connect gateways
        tools::connect_host(1, "a", "b");
        tools::connect_host(1, "a", "c");
        tools::connect_host(1, "a", "d");

        tools::connect_host(1, "b", "a");
        tools::connect_host(1, "b", "c");

        tools::connect_host(1, "c", "a");
        tools::connect_host(1, "c", "b");
        tools::connect_host(1, "c", "d");

        tools::connect_host(1, "d", "a");
        tools::connect_host(1, "d", "c");

        tools::connect_host(1, "A0", "a", behind_nat);
        tools::connect_host(1, "A1", "a", behind_nat);

        tools::connect_host(1, "B0", "b", behind_nat);
        tools::connect_host(1, "B1", "b", behind_nat);

        tools::connect_host(1, "C0", "c", behind_nat);
        tools::connect_host(1, "C1", "c", behind_nat);

        tools::connect_host(1, "D0", "d", behind_nat);
        tools::connect_host(1, "D1", "d", behind_nat);

        tools::connect_host(1, "A0", "A1");
        tools::connect_host(1, "A1", "A0");
        tools::connect_host(1, "B0", "B1");
        tools::connect_host(1, "B1", "B0");
        tools::connect_host(1, "C0", "C1");
        tools::connect_host(1, "C1", "C0");
        tools::connect_host(1, "D0", "D1");
        tools::connect_host(1, "D1", "D0");

        signal(SIGINT, tools::sigterm_handler);

        tools::run_all();
        tools::sleep(1, "Check channels established");
        tools::interrupt_all();
        tools::join_all();

        tools::print_matrix2(*s_route_matrix_5.rlock(), {"a", "b", "c", "d"
            , "A0", "A1", "B0", "B1", "C0", "C1", "D0", "D1"});

        // CHECK_EQ(channels_established_counter.load(), 14);
        // CHECK_EQ(s_route_matrix.rlock()->count(), 30);

        tools::clear();

        END_TEST_MESSAGE
    }
}
#endif
