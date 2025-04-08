////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.11 Initial version.
//      2025.04.07 Moved to tools.hpp.
////////////////////////////////////////////////////////////////////////////////
#include "bit_matrix.hpp"
#include "node.hpp"
#include <pfs/assert.hpp>
#include <pfs/countdown_timer.hpp>
#include <pfs/fmt.hpp>
#include <pfs/log.hpp>
#include <pfs/synchronized.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <vector>
#include <signal.h>

#define COLOR(x) "\033[" #x "m"
#define BALCK     COLOR(0;30)
#define DGRAY     COLOR(1;30)
#define BLUE      COLOR(0;34)
#define LBLUE     COLOR(1;34)
#define PURPLE    COLOR(0;35)
#define LPURPLE   COLOR(1;35)
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
bool wait_atomic_counter (AtomicCounter & counter
    , typename AtomicCounter::value_type limit
    , std::chrono::milliseconds timelimit = std::chrono::milliseconds{5000})
{
    pfs::countdown_timer<std::milli> timer {timelimit};

    while (counter.load() < limit && timer.remain_count() > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

    return !(counter.load() < limit);
}

template <typename SyncRouteMatrix>
bool wait_matrix_count (SyncRouteMatrix & safe_matrix, std::size_t limit
, std::chrono::milliseconds timelimit = std::chrono::milliseconds{5000})
{
    pfs::countdown_timer<std::milli> timer {timelimit};

    while (safe_matrix.rlock()->count() < limit && timer.remain_count() > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

    return !(safe_matrix.rlock()->count() < limit);
}

class signal_guard
{
    int sig {0};
    sighandler_t old_handler;

public:
    signal_guard (int sig, sighandler_t handler)
        : sig(sig)
    {
        old_handler = signal(sig, handler);
    }

    ~signal_guard ()
    {
        signal(sig, old_handler);
    }
};

class mesh_network;

class node_pool_dictionary
{
public:
    struct entry
    {
        node_pool_t::options opts;
        std::uint16_t port;
    };

private:
    std::unordered_map<std::string, entry> _data;

public:
    node_pool_dictionary (std::initializer_list<entry> init)
    {
        for (auto && x: init)
            _data.insert({x.opts.name, std::move(x)});
    }

public:
    entry const * locate (std::string const & name) const
    {
        auto pos = _data.find(name);
        PFS__ASSERT(pos != _data.end(), "");
        return & pos->second;
    }

    entry const * locate (node_pool_t::node_id id) const
    {
        entry const * ptr = nullptr;

        for (auto const & x: _data) {
            if (x.second.opts.id == id) {
                ptr = & x.second;
                break;
            }
        }

        PFS__ASSERT(ptr != nullptr, "");
        return ptr;
    }

    entry const * locate (node_pool_t::node_id_rep id_rep) const
    {
        return locate(node_pool_t::node_id_traits::cast(id_rep));
    }

    std::shared_ptr<node_pool_t>
    create_node_pool (std::string const & name, mesh_network * this_net) const;
};

constexpr bool GATEWAY_FLAG = true;
constexpr bool REGULAR_NODE_FLAG = false;

class mesh_network
{
private:
    struct context {
        std::shared_ptr<node_pool_t> np_ptr;
        std::size_t serial_number; // Serial number used in matrix to check tests results
    };

private:
    node_pool_dictionary _np_dictionary;
    std::unordered_map<std::string, context> _node_pools;
    std::vector<std::thread> _threads;

public:
    void on_channel_established (std::string const & source_name, node_t::node_id_rep id_rep
        , bool is_gateway);
    void on_channel_destroyed (std::string const & source_name, node_t::node_id_rep id_rep);
    void on_node_alive (std::string const & source_name, node_t::node_id_rep id_rep);
    void on_node_expired (std::string const & source_name, node_t::node_id_rep id_rep);
    void on_route_ready (std::string const & source_name, node_t::node_id_rep dest_id_rep
        , std::uint16_t hops);

public:
    mesh_network (std::initializer_list<std::string> np_names)
        : _np_dictionary {
            // Gateways
              { {"01JQN2NGY47H3R81Y9SG0F0A00"_uuid, "a", GATEWAY_FLAG}, 4210 }
            , { {"01JQN2NGY47H3R81Y9SG0F0B00"_uuid, "b", GATEWAY_FLAG}, 4220 }
            , { {"01JQN2NGY47H3R81Y9SG0F0C00"_uuid, "c", GATEWAY_FLAG}, 4230 }
            , { {"01JQN2NGY47H3R81Y9SG0F0D00"_uuid, "d", GATEWAY_FLAG}, 4240 }

            // Regular nodes
            , { {"01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", REGULAR_NODE_FLAG}, 4211 }
            , { {"01JQC29M6RC2EVS1ZST11P0VA1"_uuid, "A1", REGULAR_NODE_FLAG}, 4212 }
            , { {"01JQC29M6RC2EVS1ZST11P0VB0"_uuid, "B0", REGULAR_NODE_FLAG}, 4221 }
            , { {"01JQC29M6RC2EVS1ZST11P0VB1"_uuid, "B1", REGULAR_NODE_FLAG}, 4222 }
            , { {"01JQC29M6RC2EVS1ZST11P0VC0"_uuid, "C0", REGULAR_NODE_FLAG}, 4231 }
            , { {"01JQC29M6RC2EVS1ZST11P0VC1"_uuid, "C1", REGULAR_NODE_FLAG}, 4232 }
            , { {"01JQC29M6RC2EVS1ZST11P0VD0"_uuid, "D0", REGULAR_NODE_FLAG}, 4241 }
            , { {"01JQC29M6RC2EVS1ZST11P0VD1"_uuid, "D1", REGULAR_NODE_FLAG}, 4242 }
        }
    {
        std::size_t seria_number = 0;

        for (auto const & name: np_names) {
            auto np_ptr = _np_dictionary.create_node_pool(name, this);
            PFS__ASSERT(np_ptr != nullptr, "");
            _node_pools.insert({name, {np_ptr, seria_number++}});
        }
    }

private:
    context * locate (std::string const & name)
    {
        auto pos = _node_pools.find(name);
        PFS__ASSERT(pos != _node_pools.end(), "");
        return & pos->second;
    }

    context * locate (node_pool_t::node_id_rep id_rep)
    {
        context * ptr = nullptr;

        for (auto & x: _node_pools) {
            if (x.second.np_ptr->id_rep() == id_rep) {
                ptr = & x.second;
                break;
            }
        }

        PFS__ASSERT(ptr != nullptr, "");
        return ptr;
    }

public:
    std::string node_name_by_id (node_pool_t::node_id_rep id_rep)
    {
        return _np_dictionary.locate(id_rep)->opts.name;
    }

    std::string node_name_by_id (node_pool_t::node_id id)
    {
        return _np_dictionary.locate(id)->opts.name;
    }

    node_pool_t::node_id node_id_by_name (std::string const & name)
    {
        return locate(name)->np_ptr->id();
    }

    std::size_t serial_number (node_pool_t::node_id_rep id_rep)
    {
        return locate(id_rep)->serial_number;
    }

    std::size_t serial_number (std::string const & name)
    {
        return locate(name)->serial_number;
    }

    void connect_host (std::string const & initiator_name, std::string const & target_name
        , bool behind_nat = false)
    {
        meshnet::node_index_t index = 1;
        auto initiator_ctx = locate(initiator_name);
        auto target_entry_ptr = _np_dictionary.locate(target_name);

        netty::socket4_addr target_saddr {netty::inet4_addr {127, 0, 0, 1}, target_entry_ptr->port};
        initiator_ctx->np_ptr->connect_host(index, target_saddr, behind_nat);
    }

    void run_all ()
    {
        for (auto & x: _node_pools) {
            auto ptr = x.second.np_ptr;

            _threads.emplace_back([ptr] () {
                ptr->run();
            });
        }
    }

    void interrupt (std::string const & name)
    {
        auto ptr = locate(name);
        PFS__ASSERT(ptr != nullptr, "");
        ptr->np_ptr->interrupt();
    }

    void interrupt_all ()
    {
        for (auto & x: _node_pools)
            x.second.np_ptr->interrupt();
    }

    void join_all ()
    {
        for (auto & t: _threads)
            t.join();
    }
};


std::shared_ptr<node_pool_t>
node_pool_dictionary::create_node_pool (std::string const & name, mesh_network * this_meshnet) const
{
    auto const * p = locate(name);

    if (p == nullptr)
        return std::shared_ptr<node_pool_t>{};

    node_pool_t::options opts = p->opts;
    netty::socket4_addr listener_saddr {netty::inet4_addr {127, 0, 0, 1}, p->port};

    node_pool_t::callback_suite callbacks;

    callbacks.on_error = [] (std::string const & errstr) {
        LOGE(TAG, "{}", errstr);
    };

    callbacks.on_channel_established = [this_meshnet, name] (node_t::node_id_rep id_rep, bool is_gateway) {
        this_meshnet->on_channel_established(name, id_rep, is_gateway);
    };

    callbacks.on_channel_destroyed = [this_meshnet, name] (node_t::node_id_rep id_rep) {
        this_meshnet->on_channel_destroyed(name, id_rep);
    };

    callbacks.on_node_alive = [this_meshnet, name] (node_t::node_id_rep id_rep) {
        this_meshnet->on_node_alive(name, id_rep);
    };

    callbacks.on_node_expired = [this_meshnet, name] (node_t::node_id_rep id_rep) {
        this_meshnet->on_node_expired(name, id_rep);
    };

    // Notify when node alive status changed
    callbacks.on_route_ready = [this_meshnet, name] (node_t::node_id_rep dest_id_rep, std::uint16_t hops) {
        this_meshnet->on_route_ready(name, dest_id_rep, hops);
    };

    auto ptr = std::make_shared<node_pool_t>(std::move(opts), std::move(callbacks));
    auto index = ptr->add_node<node_t>({listener_saddr});

    ptr->listen(index, 10);
    return ptr;
}

template <typename RouteMatrix>
bool print_matrix_with_check (RouteMatrix & m, std::vector<char const *> caption)
{
    bool success = true;
    fmt::print("[   ]");

    for (std::size_t j = 0; j < m.columns(); j++)
        fmt::print("[{:^3}]", caption[j]);

    fmt::println("");

    for (std::size_t i = 0; i < m.rows(); i++) {
        fmt::print("[{:^3}]", caption[i]);

        for (std::size_t j = 0; j < m.columns(); j++) {
            if (i == j) {


                if (m.test(i, j)) {
                    fmt::print("[!!!]");
                    success = false;
                } else {
                    fmt::print("[---]");
                }
            } else if (m.test(i, j))
                fmt::print("[{:^3}]", '+');
            else
                fmt::print("[   ]");
        }

        fmt::println("");
    }

    return success;
}

} // namespace tools
