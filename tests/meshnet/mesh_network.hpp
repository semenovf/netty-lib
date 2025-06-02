////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.05.12 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../colorize.hpp"
#include "transport.hpp"
#include <pfs/assert.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <map>
#include <memory>
#include <vector>

static constexpr char const * TAG = CYAN "test::meshnet" END_COLOR;

namespace test {
namespace meshnet {

constexpr bool GATEWAY_FLAG = true;
constexpr bool REGULAR_NODE_FLAG = false;

class node_dictionary
{
public:
    using node_id = node_pool_t::node_id;

    struct entry
    {
        node_id id;
        std::string name;
        bool is_gateway {false};
        std::uint16_t port {0};
    };

private:
    std::map<std::string, entry> _data;

private:
    node_dictionary (std::initializer_list<entry> init)
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

    entry const * locate_by_id (node_id id) const
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

public: // static
    static std::unique_ptr<node_dictionary> make ()
    {
        // return std::make_unique<node_dictionary>(
        auto dict = new node_dictionary {
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
        };

        return std::unique_ptr<node_dictionary>{dict};
    }
};

template <typename NodePool>
class network
{
public:
    using node_id = typename NodePool::node_id;

#ifdef NETTY__TESTS_USE_MESHNET_NODE_POOL_RD
    using message_id = typename NodePool::message_id;
#endif

private:
    static network * _self;

protected:
    struct context
    {
        std::unique_ptr<NodePool> node_pool_ptr;
        std::size_t index {0}; // Index used in matrix to check tests results
    };

protected:
    std::unique_ptr<node_dictionary> _dict;
    std::map<std::string, context> _node_pools;
    std::map<NodePool *, std::thread> _threads;

public:
    netty::callback_t<void (std::string const &, std::string const &, bool)> on_channel_established
        = [] (std::string const & /*source_name*/, std::string const & /*target_name*/, bool /*is_gateway*/) {};

    netty::callback_t<void (std::string const &, std::string const &)> on_channel_destroyed
        = [] (std::string const & /*source_name*/, std::string const & /*target_name*/) {};

    netty::callback_t<void (std::string const &, std::string const &, netty::socket4_addr saddr)> on_duplicated
        = [] (std::string const & /*source_name*/, std::string const & /*target_name*/, netty::socket4_addr saddr) {};

    netty::callback_t<void (std::string const &, std::string const &)> on_node_alive
        = [] (std::string const & /*source_name*/, std::string const & /*target_name*/) {};

    netty::callback_t<void (std::string const &, std::string const &)> on_node_expired
        = [] (std::string const & /*source_name*/, std::string const & /*target_name*/) {};

    netty::callback_t<void (std::string const &, std::string const &, std::vector<node_id> const &
        , std::size_t, std::size_t)> on_route_ready
        = [] (std::string const & /*source_name*/, std::string const & /*target_name*/
            , std::vector<node_id> const & /*gw_chain*/, std::size_t /*source_index*/, std::size_t /*target_index*/) {};

#ifdef NETTY__TESTS_USE_MESHNET_NODE_POOL_RD
    netty::callback_t<void (std::string const &, std::string const &, std::size_t, std::size_t)> on_receiver_ready
        = [] (std::string const & /*source_name*/, std::string const & /*receiver_name*/
            , std::size_t /*source_index*/, std::size_t /*receiver_index*/) {};

    netty::callback_t<void (std::string const &, std::string const &, std::string const &
        , std::vector<char>)> on_message_received
        = [] (std::string const &, std::string const &, std::string const &, std::vector<char>) {};

    netty::callback_t<void (std::string const &, std::string const &, std::string const &)> on_message_delivered
        = [] (std::string const & /*source_name*/, std::string const & /*receiver_name*/
            , std::string const & /*msgid*/) {};

    netty::callback_t<void (std::string const &, std::string const &, std::vector<char>)> on_report_received
        = [] (std::string const &, std::string const &, std::vector<char>) {};

#else
    netty::callback_t<void (std::string const &, std::string const &, int, std::vector<char>
        , std::size_t, std::size_t)> on_data_received
        = [] (std::string const & /*receiver_name*/, std::string const & /*sender_name*/
            , int /*priority*/, std::vector<char> /*bytes*/, std::size_t /*source_index*/
            , std::size_t /*target_index*/) {};
#endif

public:
    network (std::initializer_list<std::string> node_pool_names)
        : _dict(node_dictionary::make())
    {
        PFS__ASSERT(_self == nullptr, "Network instance already instantiated");

        _self = this;

        std::size_t index = 0; // May be used as matrix column/row index

        for (auto const & name: node_pool_names) {
            auto node_pool_ptr = create_node_pool(name);
            PFS__ASSERT(node_pool_ptr != nullptr, "");
            _node_pools.insert({name, context{std::move(node_pool_ptr), index++}});
        }
    }

    ~network ()
    {
        PFS__ASSERT(_self != nullptr, "Network instance is not instantiated");
        _self = nullptr;
    }

private:
    auto create_node_pool (std::string const & source_name) const -> std::unique_ptr<NodePool>
    {
        auto const * p = _dict->locate_by_name(source_name);

        if (p == nullptr)
            return nullptr;

        netty::socket4_addr listener_saddr {netty::inet4_addr {127, 0, 0, 1}, p->port};

        auto ptr = std::make_unique<NodePool>(p->id, p->name, p->is_gateway);

        ptr->on_error = [] (std::string const & errstr)
        {
            LOGE(TAG, "{}", errstr);
        };

        ptr->on_channel_established = [this, source_name] (node_id id, std::string const & name
            , bool is_gateway)
        {
            this->on_channel_established(source_name, name, is_gateway);
        };

        ptr->on_channel_destroyed = [this, source_name] (node_id id)
        {
            this->on_channel_destroyed(source_name, node_name_by_id(id));
        };

        ptr->on_duplicated = [this, source_name] (node_id id, std::string const & /*name*/
            , netty::socket4_addr saddr)
        {
            this->on_duplicated(source_name, node_name_by_id(id), saddr);
        };

        ptr->on_node_alive = [this, source_name] (node_id id)
        {
            this->on_node_alive(source_name, node_name_by_id(id));
        };

        ptr->on_node_expired = [this, source_name] (node_id id)
        {
            this->on_node_expired(source_name, node_name_by_id(id));
        };

        ptr->on_route_ready = [this, source_name] (node_id target_id, std::vector<node_id> gw_chain)
        {
            auto target_name = node_name_by_id(target_id);
            this->on_route_ready(source_name, target_name, std::move(gw_chain), index_by_name(source_name)
                , index_by_name(target_name));
        };

#ifdef NETTY__TESTS_USE_MESHNET_NODE_POOL_RD
        ptr->on_receiver_ready = [this, source_name] (node_id receiver_id)
        {
            auto receiver_name = node_name_by_id(receiver_id);
            this->on_receiver_ready(source_name, receiver_name, index_by_name(source_name)
                , index_by_name(receiver_name));
        };

        ptr->on_message_received = [this, source_name] (node_id sender_id, message_id msgid
            , std::vector<char> msg)
        {
            auto sender_name = node_name_by_id(sender_id);
            this->on_message_received(source_name, sender_name, to_string(msgid), std::move(msg));
        };

        ptr->on_message_delivered = [this, source_name] (node_id receiver_id, message_id msgid)
        {
            auto receiver_name = node_name_by_id(receiver_id);
            this->on_message_delivered(source_name, receiver_name, to_string(msgid));
        };

        ptr->on_report_received = [this, source_name] (node_id sender_id, std::vector<char> report)
        {
            auto sender_name = node_name_by_id(sender_id);
            this->on_report_received(source_name, sender_name, std::move(report));
        };
#else
        ptr->on_data_received = [this, source_name] (node_id sender_id, int priority
            , std::vector<char> bytes)
        {
            auto sender_name = node_name_by_id(sender_id);
            this->on_data_received(source_name, sender_name, priority, std::move(bytes)
                , index_by_name(source_name), index_by_name(sender_name));
        };
#endif

        auto index = ptr->template add_node<node_t>({listener_saddr});

        int backlog = 10;
        ptr->listen(index, backlog);
        return ptr;
    }

protected:
    context const * locate_by_name (std::string const & name) const
    {
        auto pos = _node_pools.find(name);
        PFS__ASSERT(pos != _node_pools.end(), "");
        return & pos->second;
    }

    context const * locate_by_id (node_id id) const
    {
        context * ptr = nullptr;

        for (auto & x: _node_pools) {
            if (x.second.node_ptr->id() == id) {
                ptr = & x.second;
                break;
            }
        }

        PFS__ASSERT(ptr != nullptr, "");
        return ptr;
    }

    std::string node_name_by_id (node_id id) const
    {
        return _dict->locate_by_id(id)->name;
    }

    node_id node_id_by_name (std::string const & name)
    {
        return _dict->locate_by_name(name)->id;
    }

    std::size_t index_by_name (std::string const & name) const
    {
        return locate_by_name(name)->index;
    }

public:
    void connect_host (std::string const & initiator_name, std::string const & target_name
        , bool behind_nat = false)
    {
        meshnet_ns::node_index_t index = 1;
        auto initiator_ctx = locate_by_name(initiator_name);
        auto target_entry_ptr = _dict->locate_by_name(target_name);

        netty::socket4_addr target_saddr {netty::inet4_addr {127, 0, 0, 1}, target_entry_ptr->port};
        initiator_ctx->node_pool_ptr->connect_host(index, target_saddr, behind_nat);
    }

    void send (std::string const & src, std::string const & dest, std::string const & text)
    {
        int priority = 1;
        auto sender_ctx = locate_by_name(src);
        auto receiver_id = node_id_by_name(dest);

#ifdef NETTY__TESTS_USE_MESHNET_NODE_POOL_RD
        message_id msgid = pfs::generate_uuid();

        sender_ctx->node_pool_ptr->enqueue_message(receiver_id, msgid, priority, false
            , text.data(), text.size());
#else
        sender_ctx->node_pool_ptr->enqueue(receiver_id, priority, text.data(), text.size());
#endif
    }

    void run_all ()
    {
        for (auto & x: _node_pools) {
            auto ptr = & *x.second.node_pool_ptr;
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

        ctx_ptr->node_pool_ptr->interrupt();
        auto ptr = & *ctx_ptr->node_pool_ptr;

        auto pos = _threads.find(ptr);

        PFS__ASSERT(pos != _threads.end(), "");

        pos->second.join();

        _threads.erase(pos);
        _node_pools.erase(name);
    }

    void print_routing_table (std::string const & name)
    {
        auto ptr = locate_by_name(name);
        PFS__ASSERT(ptr != nullptr, "");

        auto routes = ptr->node_pool_ptr->dump_routing_table();

        LOGD(TAG, "┌────────────────────────────────────────────────────────────────────────────────");
        LOGD(TAG, "│Routes for: {}", name);

        for (auto const & x: routes) {
            LOGD(TAG, "│    └──── {}", x);
        }

        LOGD(TAG, "└────────────────────────────────────────────────────────────────────────────────");
    }

    void interrupt (std::string const & name)
    {
        auto ptr = locate_by_name(name);
        PFS__ASSERT(ptr != nullptr, "");
        ptr->node_pool_ptr->interrupt();
    }

    void interrupt_all ()
    {
        for (auto & x: _node_pools)
            x.second.node_pool_ptr->interrupt();
    }

    void join_all ()
    {
        for (auto & t: _threads) {
            if (t.second.joinable())
                t.second.join();
        }
    }

public: // static
    static network * instance ()
    {
        PFS__ASSERT(_self != nullptr, "Network instance is null");
        return _self;
    }
};

template <typename Node>
network<Node> * network<Node>::_self {nullptr};

}} // namespace test::mesh_network
