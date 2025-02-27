////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "channel_id.hpp"
#include "channel_interface.hpp"
#include "functional_callbacks.hpp"
#include <pfs/countdown_timer.hpp>
#include <pfs/i18n.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdTraits
    , typename RoutingTable
    , template <typename> class CallbackSuite
    , typename Loggable>
class node: public Loggable
{
public:
    using node_id_traits = NodeIdTraits;
    using node_id = typename NodeIdTraits::node_id;
    using callback_suite = CallbackSuite<node_id>;

private:
    using channel_interface_type = channel_interface<NodeIdTraits>;
    using channel_interface_ptr = std::unique_ptr<channel_interface_type>;
    using routing_table_type = RoutingTable;

private:
    node_id _id;
    std::pair<std::uint64_t, std::uint64_t> _id_pair; // Node ID representation

    // True if the node is behind NAT
    bool _behind_nat {false};

    // There will rarely be more than two channels, so vector is a optimal choice
    std::vector<channel_interface_ptr> _channels;

    routing_table_type _rtab;
    callback_suite _callbacks;

    std::shared_ptr<functional_channel_callbacks<node_id>> _channel_callbacks;

public:
    node (node_id id, bool behind_nat, callback_suite && callbacks)
        : Loggable()
        , _id(id)
        , _behind_nat(behind_nat)
        , _callbacks(std::move(callbacks))
    {
        _id_pair.first = node_id_traits::high(_id);
        _id_pair.second = node_id_traits::low(_id);

        _channel_callbacks = std::make_shared<functional_channel_callbacks<node_id>>();

        _channel_callbacks->on_channel_established = [this] (node_id id) {
            _callbacks.on_channel_established(id);
        };

        _channel_callbacks->on_channel_destroyed = [this] (node_id id) {
            _callbacks.on_channel_destroyed(id);
        };

        _channel_callbacks->on_bytes_written = [this] (node_id id, std::uint64_t n) {
            _callbacks.on_bytes_written(id, n);
        };

        _channel_callbacks->on_route_received = [this] (node_id, bool is_response
            , std::vector<std::pair<std::uint64_t, std::uint64_t>> && route) {
            _rtab.process(is_response, std::move(route));
        };

        _channel_callbacks->on_message_received = [this] (node_id id, int priority, std::vector<char> && bytes) {
            _callbacks.on_message_received(id, priority, std::move(bytes));
        };

        _channel_callbacks->on_foreign_message_received = [this] (node_id id
            , int priority
            , std::pair<std::uint64_t, std::uint64_t> sender_id
            , std::pair<std::uint64_t, std::uint64_t> receiver_id
            , std::vector<char> && bytes) {

            // I am a message receiver
            if (receiver_id == _id_pair) {
                _callbacks.on_message_received(node_id_traits::make(sender_id.first, sender_id.second)
                    , priority, std::move(bytes));
            } else {
                // TODO retransmit this message to the receiver
            }
        };
    }

    node (node const &) = delete;
    node (node &&) = delete;
    node & operator = (node const &) = delete;
    node & operator = (node &&) = delete;

    ~node () = default;

private:
    channel_interface_type * locate_writer (node_id id)
    {
        if (_channels.empty())
            return nullptr;

        // TODO First check routing table
        //...

        if (_channels[0]->has_writer(id)) {
            // TODO Update routing table
            // ...

            return & *_channels[0];
        }

        for (int i = 1; i < _channels.size(); i++) {
            if (_channels[i]->has_writer(id)) {
                // TODO Update routing table
                // ...

                return & *_channels[i];
            }
        }

        return nullptr;
    }

    channel_interface_type * locate_channel (channel_id cid)
    {
        if (cid < 1 && cid > _channels.size()) {
            this->log_error(tr::f_("channel identifier is out of bounds: {}", cid));
            return nullptr;
        }

        return & *_channels[cid - 1];
    }

public:
    node_id id () const noexcept
    {
        return _id;
    }

    bool is_behind_nat () const noexcept
    {
        return _behind_nat;
    }

    /**
     * Adds new channel to the node with specified listeners.
     */
    template <typename Channel, typename ListenerIt>
    channel_id add_channel (ListenerIt first, ListenerIt last, error * perr = nullptr)
    {
        static_assert(std::is_same<typename Channel::callback_suite, functional_channel_callbacks<node_id>>::value
            , "Incompatible callback for node");

        auto channel = Channel::template make_interface(_id, _behind_nat, _channel_callbacks);
        error err;

        for (ListenerIt pos = first; pos != last; ++pos) {
            channel->add_listener(*pos, & err);

            if (err)
                pfs::throw_or(perr, std::move(err));
        }

        _channels.push_back(std::move(channel));
        return static_cast<channel_id>(_channels.size());
    }

    /**
     * Adds new channel to the node with specified listeners.
     */
    template <typename Channel>
    channel_id add_channel (std::vector<socket4_addr> const & listener_saddrs, error * perr = nullptr)
    {
        return add_channel<Channel>(listener_saddrs.begin(), listener_saddrs.end(), perr);
    }

    /**
     * Adds new channel to the node with specified listeners.
     */
    template <typename Channel>
    channel_id add_channel (std::initializer_list<socket4_addr> const & listener_saddrs, error * perr = nullptr)
    {
        return add_channel<Channel>(listener_saddrs.begin(), listener_saddrs.end(), perr);
    }

    void remove_channel (channel_id cid)
    {
        if (cid < 1 && cid > _channels.size())
            return;

        return _channels.erase(cid - 1);
    }

    bool connect_host (channel_id cid, netty::socket4_addr remote_saddr)
    {
        auto ptr = locate_channel(cid);

        if (ptr == nullptr)
            return false;

        return ptr->connect_host(remote_saddr);
    }

    bool connect_host (channel_id cid, netty::socket4_addr remote_saddr, netty::inet4_addr local_addr)
    {
        auto ptr = locate_channel(cid);

        if (ptr == nullptr)
            return false;

        return ptr->connect_host(remote_saddr, local_addr);
    }

    /**
     * Equeues message for delivery to specified node ID @a id.
     *
     * @param id Receiver ID.
     * @param priority Message priority.
     * @param force_checksum Calculate checksum before sending.
     * @param data Message content.
     * @param len Length of the message content.
     *
     * @return @c true if message route found for @a id.
     */
    bool enqueue (node_id id, int priority, bool force_checksum, char const * data, std::size_t len)
    {
        auto ptr = locate_writer(id);

        if (ptr == nullptr) {
            this->log_error(tr::f_("node not found to send message: {}", NodeIdTraits::stringify(id)));
            return false;
        }

        ptr->enqueue(id, priority, force_checksum, data, len);
        return true;
    }

    /**
     * Equeues message for delivery to specified node ID @a id.
     *
     * @param id Receiver ID.
     * @param priority Message priority.
     * @param data Message content.
     * @param len Length of the message content.
     *
     * @return @c true if message route found for @a id.
     */
    bool enqueue (node_id id, int priority, char const * data, std::size_t len)
    {
        return enqueue(id, priority, false, data, len);
    }

    void listen (int backlog = 50)
    {
        for (auto & x: _channels)
            x->listen(backlog);
    }

    void step (std::chrono::milliseconds millis)
    {
        pfs::countdown_timer<std::milli> countdown_timer {millis};

        for (auto & x: _channels)
            x->step();

        std::this_thread::sleep_for(countdown_timer.remain());
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
