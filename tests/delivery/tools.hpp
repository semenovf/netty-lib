////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.04.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "../colorize.hpp"

#ifndef TAG_DEFINED
#   define TAG_DEFINED 1
static constexpr char const * TAG = CYAN "delivery-test" END_COLOR;
#endif

#include "../meshnet/tools.hpp"
#include <pfs/universal_id_hash.hpp>
#include <pfs/universal_id_traits.hpp>
#include <pfs/netty/patterns/meshnet/node_pool_rd.hpp>
#include <pfs/netty/patterns/delivery/incoming_controller.hpp>
#include <pfs/netty/patterns/delivery/manager.hpp>
#include <pfs/netty/patterns/delivery/outgoing_controller.hpp>
// #include <map>
// #include <memory>
// #include <mutex>
// #include <thread>

namespace tools {

namespace delivery_ns = netty::patterns::delivery;
namespace meshnet_ns = netty::patterns::meshnet;

using delivery_transport_t = node_pool_t;
using message_id_traits = pfs::universal_id_traits;
using incoming_controller_t = delivery_ns::incoming_controller<message_id_traits
    , netty::patterns::serializer_traits_t>;
using outgoing_controller_t = delivery_ns::outgoing_controller<message_id_traits
    , netty::patterns::serializer_traits_t>;
using delivery_manager_t = delivery_ns::manager<delivery_transport_t, message_id_traits
    , incoming_controller_t, outgoing_controller_t, std::mutex>;

using meshnet_node_t = meshnet_ns::node_pool_rd<delivery_manager_t>;

class mesh_network_delivery: protected mesh_network
{
    friend class mesh_network;

// private:
//     std::map<std::string, std::unique_ptr<delivery_manager_t>> _with_delivery_manager;

public:
    mesh_network_delivery (std::initializer_list<std::string> np_names)
        : mesh_network(std::move(np_names))
    {
        _meshnet_delivery_ptr = this;
    }

// private:
//     bool with_delivery_manager (std::string const & np_name) const
//     {
//         auto found = _with_delivery_manager.find(np_name) != _with_delivery_manager.end();
//         return found;
//     }
//
//     delivery_manager_t * delivery_manager (std::string const & np_name)
//     {
//         auto pos = _with_delivery_manager.find(np_name);
//         PFS__ASSERT(pos != _with_delivery_manager.end(), "");
//         return & *pos->second;
//     }

public:
//     void on_receiver_ready (std::string const & receiver_name, delivery_manager_t::address_type addr);
//     void on_message_received (std::string const & sender_name, delivery_manager_t::address_type addr
//         , delivery_manager_t::message_id msgid, std::vector<char> msg);
//     void on_message_delivered (std::string const & receiver_name, delivery_manager_t::address_type addr
//         , delivery_manager_t::message_id msgid);
//     void on_report_received (std::string const & sender_name, delivery_manager_t::address_type addr
//         , std::vector<char> report);
//
// public:
//     std::string node_name_by_id (node_pool_t::node_id id)
//     {
//         return mesh_network::node_name_by_id(id);
//     }
//
//     void tie_delivery_manager (std::string np_name)
//     {
//         auto pdm = std::make_unique<delivery_manager_t>(transport(np_name));
//
//         pdm->on_receiver_ready = [this] (delivery_manager_t::address_type addr)
//         {
//             this->on_receiver_ready(this->node_name_by_id(addr), addr);
//         };
//
//         pdm->on_message_received = [this] (delivery_manager_t::address_type addr
//             , tools::delivery_manager_t::message_id msgid, std::vector<char> msg)
//         {
//             this->on_message_received(this->node_name_by_id(addr), addr, msgid, std::move(msg));
//         };
//
//         pdm->on_message_delivered = [this] (delivery_manager_t::address_type addr
//             , tools::delivery_manager_t::message_id msgid)
//         {
//             this->on_message_delivered(this->node_name_by_id(addr), addr, msgid);
//         };
//
//         pdm->on_report_received = [this] (delivery_manager_t::address_type addr
//             , std::vector<char> report)
//         {
//             this->on_report_received(this->node_name_by_id(addr), addr, std::move(report));
//         };
//
//         _with_delivery_manager.insert({std::move(np_name), std::move(pdm)});
//     }
//
//     void connect_host (std::string const & initiator_name, std::string const & target_name
//         , bool behind_nat = false)
//     {
//         mesh_network::connect_host(initiator_name, target_name, behind_nat);
//     }
//
//     void send (std::string const & src, std::string const & dest, std::string const & text)
//     {
//         int priority = 1;
//         bool force_checksum = false;
//         message_id_traits::type msgid = pfs::generate_uuid();
//
//         delivery_manager(src)->enqueue_message(node_id_by_name(dest), msgid, priority
//             , force_checksum, std::vector<char>{text.begin(), text.end()});
//     }
//
//     void send_report (std::string const & src, std::string const & dest, std::string const & text)
//     {
//         int priority = 1;
//         bool force_checksum = false;
//         message_id_traits::type msgid = pfs::generate_uuid();
//
//         delivery_manager(src)->enqueue_report(node_id_by_name(dest), priority, force_checksum
//             , std::vector<char>{text.begin(), text.end()});
//     }
//
//     void run_all ()
//     {
//         for (auto & x: _node_pools) {
//             auto ptr = & *x.second.np_ptr;
//
//             if (with_delivery_manager(x.first)) {
//                 auto dm_ptr = & *_with_delivery_manager.find(x.first)->second;
//
//                 std::thread th {
//                     [dm_ptr, ptr] () {
//                         LOGD(TAG, "{}: delivery manager thread started", ptr->name());
//                         dm_ptr->run();
//                         LOGD(TAG, "{}: delivery manager thread finished", ptr->name());
//                     }
//                 };
//
//                 _threads.insert({ptr, std::move(th)});
//             } else {
//                 std::thread th {
//                     [ptr] () {
//                         LOGD(TAG, "{}: thread started", ptr->name());
//                         ptr->run();
//                         LOGD(TAG, "{}: thread finished", ptr->name());
//                     }
//                 };
//
//                 _threads.insert({ptr, std::move(th)});
//             }
//         }
//     }
//
//     void interrupt_all ()
//     {
//         mesh_network::interrupt_all();
//
//         for (auto & x: _with_delivery_manager) {
//             x.second->interrupt();
//         }
//     }

    void join_all ()
    {
        for (auto & t: _threads) {
            if (t.second.joinable())
                t.second.join();
        }
    }
};

} // namespace tools
