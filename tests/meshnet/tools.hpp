////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.11 Initial version.
//      2025.04.07 Moved to tools.hpp.
////////////////////////////////////////////////////////////////////////////////
#include <pfs/assert.hpp>
#include <pfs/countdown_timer.hpp>
#include <pfs/synchronized.hpp>
#include <pfs/universal_id_hash.hpp>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <vector>

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
    std::atomic_int channels_established_counter {0};
    std::atomic_int channels_destroyed_counter {0};

    using stub_matrix_type = pfs::synchronized<bit_matrix<1>>;
    pfs::synchronized<bit_matrix<3>> s_route_matrix_1;
    pfs::synchronized<bit_matrix<5>> s_route_matrix_2;
    pfs::synchronized<bit_matrix<4>> s_route_matrix_3;
    pfs::synchronized<bit_matrix<6>> s_route_matrix_4;
    pfs::synchronized<bit_matrix<12>> s_route_matrix_5;

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

    template <typename AtomicCounter>
    bool wait_atomic_counter_limit (AtomicCounter & counter
        , typename AtomicCounter::value_type limit
        , std::chrono::milliseconds timelimit = std::chrono::milliseconds{5000})
    {
        pfs::countdown_timer<std::milli> timer {timelimit};

        while (counter.load() < limit && timer.remain_count() > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds{10});

        return !(counter.load() < limit);
    }

    template <typename RouteMatrix>
    bool wait_matrix_count (RouteMatrix & m, std::size_t limit
        , std::chrono::milliseconds timelimit = std::chrono::milliseconds{5000})
    {
        pfs::countdown_timer<std::milli> timer {timelimit};

        while (m.rlock()->count() < limit && timer.remain_count() > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds{10});

        return !(m.rlock()->count() < limit);
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
    void print_matrix (RouteMatrix & m, std::vector<char const *> caption)
    {
        fmt::print("[   ]");

        for (std::size_t j = 0; j < m.columns(); j++)
            fmt::print("[{:^3}]", caption[j]);

        fmt::println("");

        for (std::size_t i = 0; i < m.rows(); i++) {
            fmt::print("[{:^3}]", caption[i]);

            for (std::size_t j = 0; j < m.columns(); j++) {
                if (i == j) {
                    fmt::print("[XXX]");
                    REQUIRE(!m.test(i, j));
                } else if (m.test(i, j))
                    fmt::print("[{:^3}]", '+');
                else
                    fmt::print("[   ]");
            }

            fmt::println("");
        }
    }

    template <typename RouteMatrix>
    void create_node_pool (node_pool_t::node_id id, std::string name, std::uint16_t port
        , bool is_gateway, std::size_t serial_number, RouteMatrix * route_matrix)
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
        callbacks.on_route_ready = [id, name, route_matrix] (node_t::node_id_rep dest, std::uint16_t hops) {
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

            if (route_matrix != nullptr)
                route_matrix->wlock()->set(row, col, true);
        };

        auto node_pool = std::make_shared<node_pool_t>(std::move(opts), std::move(callbacks));
        auto node_index = node_pool->add_node<node_t>({listener_saddr});

        node_pool->listen(node_index, 10);

        tools::name_dictionary.wlock()->insert({node_pool->id(), name});
        tools::nodes.insert({std::move(name), {node_pool, port, serial_number}});
    }
} // namespace tools
