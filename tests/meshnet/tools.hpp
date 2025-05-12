////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.11 Initial version.
//      2025.04.07 Moved to tools.hpp.
////////////////////////////////////////////////////////////////////////////////
#include "../colorize.hpp"

#ifndef TAG_DEFINED
#   define TAG_DEFINED 1
static constexpr char const * TAG = CYAN "meshnet-test" END_COLOR;
#endif

#include "../tools.hpp"
#include "transport.hpp"
#include <pfs/assert.hpp>
#include <pfs/synchronized.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <map>
#include <vector>

namespace tools {

class mesh_network;

class node_pool_dictionary
{
public:
    struct entry
    {
        node_pool_t::node_id id;
        std::string name;
        bool is_gateway {false};
        std::uint16_t port {0};
    };

private:
    std::map<std::string, entry> _data;

public:
    node_pool_dictionary (std::initializer_list<entry> init)
    {
        for (auto && x: init) {
            auto res = _data.insert({x.name, std::move(x)});
            PFS__ASSERT(res.second, "");
            auto & ent = res.first->second;
        }
    }

public:
    entry const * locate_by_name (std::string const & name) const
    {
        auto pos = _data.find(name);
        PFS__ASSERT(pos != _data.end(), "");
        return & pos->second;
    }

    entry const * locate_by_id (node_pool_t::node_id id) const
    {
        entry const * ptr = nullptr;

        for (auto const & x: _data) {
            if (x.second.id == id) {
                ptr = & x.second;
                break;
            }
        }

        PFS__ASSERT(ptr != nullptr, "");
        return ptr;
    }

    std::unique_ptr<node_pool_t>
    create_node_pool (std::string const & name, mesh_network * this_net) const;
};

constexpr bool GATEWAY_FLAG = true;
constexpr bool REGULAR_NODE_FLAG = false;

class mesh_network
{
protected:
    struct context
    {
        std::unique_ptr<node_pool_t> np_ptr;
        std::size_t serial_number; // Serial number used in matrix to check tests results
    };

protected:
    node_pool_dictionary _np_dictionary;
    std::map<std::string, context> _node_pools;
    std::map<node_pool_t *, std::thread> _threads;

public:
    void on_channel_established (std::string const & source_name, node_t::node_id id, bool is_gateway);
    void on_channel_destroyed (std::string const & source_name, node_t::node_id id);
    void on_duplicated (std::string const & source_name, node_t::node_id id
        , std::string const & name, netty::socket4_addr saddr);

    void on_node_alive (std::string const & source_name, node_t::node_id id);
    void on_node_expired (std::string const & source_name, node_t::node_id id);
    void on_route_ready (std::string const & source_name, node_t::node_id dest_id, std::uint16_t hops);
    void on_message_received (std::string const & receiver_name, node_t::node_id sender_id
        , int priority, std::vector<char> && bytes);

public:
    mesh_network (std::initializer_list<std::string> np_names)
        : _np_dictionary {
            // Gateways
              { "01JQN2NGY47H3R81Y9SG0F0A00"_uuid, "a", GATEWAY_FLAG, 4210 }
            , { "01JQN2NGY47H3R81Y9SG0F0B00"_uuid, "b", GATEWAY_FLAG, 4220 }
            , { "01JQN2NGY47H3R81Y9SG0F0C00"_uuid, "c", GATEWAY_FLAG, 4230 }
            , { "01JQN2NGY47H3R81Y9SG0F0D00"_uuid, "d", GATEWAY_FLAG, 4240 }

            // Regular nodes
            , { "01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0", REGULAR_NODE_FLAG, 4211 }
            , { "01JQC29M6RC2EVS1ZST11P0VA1"_uuid, "A1", REGULAR_NODE_FLAG, 4212 }
            , { "01JQC29M6RC2EVS1ZST11P0VB0"_uuid, "B0", REGULAR_NODE_FLAG, 4221 }
            , { "01JQC29M6RC2EVS1ZST11P0VB1"_uuid, "B1", REGULAR_NODE_FLAG, 4222 }
            , { "01JQC29M6RC2EVS1ZST11P0VC0"_uuid, "C0", REGULAR_NODE_FLAG, 4231 }
            , { "01JQC29M6RC2EVS1ZST11P0VC1"_uuid, "C1", REGULAR_NODE_FLAG, 4232 }
            , { "01JQC29M6RC2EVS1ZST11P0VD0"_uuid, "D0", REGULAR_NODE_FLAG, 4241 }
            , { "01JQC29M6RC2EVS1ZST11P0VD1"_uuid, "D1", REGULAR_NODE_FLAG, 4242 }

            // For test duplication
            , { "01JQC29M6RC2EVS1ZST11P0VA0"_uuid, "A0_dup", REGULAR_NODE_FLAG, 4213 }
        }
    {
        std::size_t seria_number = 0;

        for (auto const & name: np_names) {
            auto np_ptr = _np_dictionary.create_node_pool(name, this);
            PFS__ASSERT(np_ptr != nullptr, "");
            _node_pools.insert({name, context{std::move(np_ptr), seria_number++}});
        }
    }

protected:
    context * locate_by_name (std::string const & name)
    {
        auto pos = _node_pools.find(name);
        PFS__ASSERT(pos != _node_pools.end(), "");
        return & pos->second;
    }

    context * locate_by_id (node_pool_t::node_id id)
    {
        context * ptr = nullptr;

        for (auto & x: _node_pools) {
            if (x.second.np_ptr->id() == id) {
                ptr = & x.second;
                break;
            }
        }

        PFS__ASSERT(ptr != nullptr, "");
        return ptr;
    }

public:
    std::string node_name_by_id (node_pool_t::node_id id)
    {
        return _np_dictionary.locate_by_id(id)->name;
    }

    node_pool_t::node_id node_id_by_name (std::string const & name)
    {
        // return locate(name)->np_ptr->id();
        return _np_dictionary.locate_by_name(name)->id;
    }

    std::size_t serial_number (node_pool_t::node_id id)
    {
        return locate_by_id(id)->serial_number;
    }

    std::size_t serial_number (std::string const & name)
    {
        return locate_by_name(name)->serial_number;
    }

    void connect_host (std::string const & initiator_name, std::string const & target_name
        , bool behind_nat = false)
    {
        meshnet_ns::node_index_t index = 1;
        auto initiator_ctx = locate_by_name(initiator_name);
        auto target_entry_ptr = _np_dictionary.locate_by_name(target_name);

        netty::socket4_addr target_saddr {netty::inet4_addr {127, 0, 0, 1}, target_entry_ptr->port};
        initiator_ctx->np_ptr->connect_host(index, target_saddr, behind_nat);
    }

    void send (std::string const & src, std::string const & dest, std::string const & text)
    {
        int priority = 1;
        auto sender_ctx = locate_by_name(src);
        auto receiver_id = node_id_by_name(dest);

        sender_ctx->np_ptr->enqueue(receiver_id, priority, text.data(), text.size());
    }

    void run_all ()
    {
        for (auto & x: _node_pools) {
            auto ptr = & *x.second.np_ptr;
            std::thread th {
                [ptr] () {
                    LOGD(TAG, "{}: thread started", ptr->name());
                    ptr->run();
                    LOGD(TAG, "{}: thread finished", ptr->name());
                }
            };

            _threads.insert({ptr, std::move(th)});
        }
    }

    /**
     * Interrupt thread associated with node pool named by @a name and destroy these node pool.
     */
    void destroy (std::string const & name)
    {
        auto ctx_ptr = locate_by_name(name);

        PFS__ASSERT(ctx_ptr != nullptr, "");

        ctx_ptr->np_ptr->interrupt();
        auto ptr = & *ctx_ptr->np_ptr;

        auto pos = _threads.find(ptr);

        PFS__ASSERT(pos != _threads.end(), "");

        pos->second.join();

        _threads.erase(pos);
        _node_pools.erase(name);
    }

    void print_routing_table (std::string const & name)
    {
#if NETTY__TRACE_ENABLED
        auto ptr = locate_by_name(name);
        PFS__ASSERT(ptr != nullptr, "");

        auto routes = ptr->np_ptr->dump_routing_table();

        LOGD(TAG, "┌────────────────────────────────────────────────────────────────────────────────");
        LOGD(TAG, "│Routes for: {}", name);

        for (auto const & x: routes) {
            LOGD(TAG, "│    └──── {}", x);
        }

        LOGD(TAG, "└────────────────────────────────────────────────────────────────────────────────");
#endif
    }

    void interrupt (std::string const & name)
    {
        auto ptr = locate_by_name(name);
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
        for (auto & t: _threads) {
            if (t.second.joinable())
                t.second.join();
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Delivery manager tests requirements
    ////////////////////////////////////////////////////////////////////////////////////////////////
    node_pool_t & transport (std::string const & name)
    {
        return *locate_by_name(name)->np_ptr;
    }
};

std::unique_ptr<node_pool_t>
node_pool_dictionary::create_node_pool (std::string const & source_name, mesh_network * this_meshnet) const
{
    auto const * p = locate_by_name(source_name);

    if (p == nullptr)
        return nullptr;

    netty::socket4_addr listener_saddr {netty::inet4_addr {127, 0, 0, 1}, p->port};

    auto ptr = std::make_unique<node_pool_t>(p->id, p->name, p->is_gateway);

    ptr->on_error = [] (std::string const & errstr) {
        LOGE(TAG, "{}", errstr);
    };

    ptr->on_channel_established = [this_meshnet, source_name] (node_t::node_id id, bool is_gateway) {
        this_meshnet->on_channel_established(source_name, id, is_gateway);
    };

    ptr->on_channel_destroyed = [this_meshnet, source_name] (node_t::node_id id) {
        this_meshnet->on_channel_destroyed(source_name, id);
    };

    ptr->on_duplicated = [this_meshnet, source_name] (node_t::node_id id
            , std::string const & name, netty::socket4_addr saddr) {
        this_meshnet->on_duplicated(source_name, id, name, saddr);
    };

    ptr->on_node_alive = [this_meshnet, source_name] (node_t::node_id id) {
        this_meshnet->on_node_alive(source_name, id);
    };

    ptr->on_node_expired = [this_meshnet, source_name] (node_t::node_id id) {
        this_meshnet->on_node_expired(source_name, id);
    };

    // Notify when node alive status changed
    ptr->on_route_ready = [this_meshnet, source_name] (node_t::node_id dest_id, std::uint16_t hops) {
        this_meshnet->on_route_ready(source_name, dest_id, hops);
    };

    ptr->on_message_received = [this_meshnet, source_name] (node_t::node_id sender_id
            , int priority, std::vector<char> bytes) {
        this_meshnet->on_message_received(source_name, sender_id, priority, std::move(bytes));
    };

    auto index = ptr->add_node<node_t>({listener_saddr});

    ptr->listen(index, 10);
    return ptr;
}

} // namespace tools
