////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../conn_status.hpp"
#include "../../connection_refused_reason.hpp"
#include "../../error.hpp"
#include "../../namespace.hpp"
#include "../../socket4_addr.hpp"
#include "../../connecting_pool.hpp"
#include "../../listener_pool.hpp"
#include "../../reader_pool.hpp"
#include "../../socket_pool.hpp"
#include "../../trace.hpp"
#include "../../writer_pool.hpp"
#include "alive_info.hpp"
#include "channel_map.hpp"
#include "handshake_result.hpp"
#include "node_index.hpp"
#include "node_interface.hpp"
#include "protocol.hpp"
#include "route_info.hpp"
#include <pfs/i18n.hpp>
#include <chrono>
#include <memory>
#include <set>
#include <thread>
#include <unordered_map>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename ChannelCollection
    , typename Listener
    , typename ConnectingPoller
    , typename ListenerPoller
    , typename ReaderPoller
    , typename WriterPoller
    , typename WriterQueue
    , typename SerializerTraits
    , typename ReconnectionPolicy
    , template <typename> class HandshakeProcessor
    , template <typename> class HeartbeatProcessor
    , template <typename> class InputProcessor
    , typename CallbackSuite>
class node
{
    friend class HandshakeProcessor<node>;
    friend class HeartbeatProcessor<node>;
    friend /*class */InputProcessor<node>;

public:
    using channel_collection_type = ChannelCollection;

private:
    using node_class = node<ChannelCollection
        , Listener
        , ConnectingPoller
        , ListenerPoller
        , ReaderPoller
        , WriterPoller
        , WriterQueue
        , SerializerTraits
        , ReconnectionPolicy
        , HandshakeProcessor
        , HeartbeatProcessor
        , InputProcessor
        , CallbackSuite>;

    using listener_type = Listener;
    using socket_type = typename channel_collection_type::socket_type;
    using socket_pool_type = netty::socket_pool<socket_type>;
    using connecting_pool_type = netty::connecting_pool<socket_type, ConnectingPoller>;
    using listener_pool_type = netty::listener_pool<listener_type, socket_type, ListenerPoller>;
    using reader_pool_type = netty::reader_pool<socket_type, ReaderPoller>;
    using writer_pool_type = netty::writer_pool<socket_type, WriterPoller, WriterQueue>;
    using listener_id = typename listener_type::listener_id;
    using reconnection_policy = ReconnectionPolicy;

public:
    using socket_id = typename socket_type::socket_id;
    using serializer_traits = SerializerTraits;
    using node_id_traits = typename channel_collection_type::node_id_traits;
    using node_id = typename node_id_traits::type;
    using node_id_rep = typename node_id_traits::rep_type;
    using callback_suite = CallbackSuite;

    struct options
    {
        node_id id;
        std::string name;
        bool is_gateway {false};
    };

private:
    // Unique node identifier
    node_id_rep _id_rep;

    // User friendly node identifier, default value is stringified representation of node identifier
    std::string _name;

    channel_collection_type _channels;

    listener_pool_type   _listener_pool;
    connecting_pool_type _connecting_pool;
    reader_pool_type     _reader_pool;
    writer_pool_type     _writer_pool;
    socket_pool_type     _socket_pool;

    // True if the node is a part of gateway
    bool _is_gateway {false};

    HandshakeProcessor<node> _handshake_processor;
    HeartbeatProcessor<node> _heartbeat_processor;
    InputProcessor<node> _input_processor;
    std::shared_ptr<CallbackSuite> _callbacks;

    std::map<socket4_addr, reconnection_policy> _reconn_policies;

    // Nodes for which the current node is behind NAT
    std::set<socket4_addr> _behind_nat;

    // Make sense when node is a part of node_pool.
    node_index_t _index {INVALID_NODE_INDEX};

public:
    node (options && opts, std::shared_ptr<CallbackSuite> callbacks)
        : _id_rep(node_id_traits::cast(opts.id))
        , _is_gateway(opts.is_gateway)
        , _handshake_processor(this, & _channels)
        , _heartbeat_processor(this)
        , _input_processor(this)
        , _callbacks(callbacks)
    {
        if (opts.name.empty())
            _name = node_id_traits::to_string(opts.id);
        else
            _name = std::move(opts.name);

        _channels.on_close_socket([this] (socket_id sid) {
            _handshake_processor.cancel(sid);
            _heartbeat_processor.remove(sid);
            _input_processor.remove(sid);
            _reader_pool.remove_later(sid);
            _writer_pool.remove_later(sid);
            _socket_pool.remove_later(sid);
        });

        _listener_pool.on_failure([this] (netty::error const & err) {
            _callbacks->on_error(tr::f_("listener pool failure: {}", err.what()));
        }).on_accepted([this] (socket_type && sock) {
            NETTY__TRACE("[node]", "{}: socket accepted: #{}: {}", _name, sock.id(), to_string(sock.saddr()));
            _input_processor.add(sock.id());
            _reader_pool.add(sock.id());
            _socket_pool.add_accepted(std::move(sock));
        });

        _connecting_pool.on_failure([this] (netty::error const & err) {
            _callbacks->on_error(tr::f_("connecting pool failure: {}", err.what()));
        }).on_connected([this] (socket_type && sock) {
            NETTY__TRACE("[node]", "{}: socket connected: #{}: {}", _name, sock.id(), to_string(sock.saddr()));

            bool behind_nat = false;

            if (_behind_nat.find(sock.saddr()) != _behind_nat.end())
                behind_nat = true;

            _handshake_processor.start(sock.id(), behind_nat);
            _input_processor.add(sock.id());
            _reader_pool.add(sock.id());
            _socket_pool.add_connected(std::move(sock));
            _reconn_policies.erase(sock.saddr());
        }).on_connection_refused ([this] (netty::socket4_addr saddr
                , netty::connection_refused_reason reason) {
            _callbacks->on_error(tr::f_("connection refused for socket: {}: reason: {}"
                , to_string(saddr), to_string(reason)));
            schedule_reconnection(saddr);
        });

        _reader_pool.on_failure([this] (socket_id sid, netty::error const & err) {
            _callbacks->on_error(tr::f_("read from socket failure: #{}: {}", sid, err.what()));
            _channels.close_channel(sid);
        }).on_disconnected([this] (socket_id sid) {
            NETTY__TRACE("[node]", "{}: reader socket disconnected: #{}", _name, sid);
            // schedule_reconnection(sid); // FIXME When need reconnection?
        }).on_data_ready([this] (socket_id sid, std::vector<char> && data) {
            _input_processor.process_input(sid, std::move(data));
        }).on_locate_socket([this] (socket_id sid) {
            return _socket_pool.locate(sid);
        });

        _writer_pool.on_failure([this] (socket_id sid, netty::error const & err) {
            _callbacks->on_error(tr::f_("write to socket failure: #{}: {}", sid, err.what()));
            schedule_reconnection(sid);
        }).on_disconnected([this] (socket_id sid) {
            NETTY__TRACE("[node]", "{}: writer socket disconnected: #{}", _name, sid);
            schedule_reconnection(sid);
        }).on_bytes_written([this] (socket_id sid, std::uint64_t n) {
            auto id_ptr = _channels.locate_writer(sid);

            if (id_ptr != nullptr)
                _callbacks->on_bytes_written(*id_ptr, n);
        }).on_locate_socket([this] (socket_id sid) {
            return _socket_pool.locate(sid);
        });

        _handshake_processor.on_expired([this] (socket_id sid) {
            NETTY__TRACE("[node]", "{}: handshake expired for socket: #{}", _name, sid);
        }).on_completed([this] (node_id_rep const & id_rep, socket_id sid, std::string const & name
                , bool is_gateway, handshake_result_enum status) {
            switch (status) {
                case handshake_result_enum::success: {
                    socket_id const * writer_sid = _channels.locate_writer(id_rep);

                    PFS__TERMINATE(writer_sid != nullptr, "Fix meshnet::node algorithm");

                    _heartbeat_processor.update(*writer_sid);
                    _callbacks->on_channel_established(id_rep, _index, is_gateway);
                    break;
                }

                case handshake_result_enum::rejected:
                    _callbacks->on_error(tr::f_("handshake rejected for: {} on socket #{}:"
                        " channel already established", name, sid));
                    break;

                case handshake_result_enum::duplicated:
                    _callbacks->on_error(tr::f_("node ID duplication with: {} on socket #{}", name, sid));
                    break;

                default:
                    PFS__TERMINATE(false, "Fix meshnet::node algorithm");
                    break;
            }
        });

        _heartbeat_processor.on_expired ([this] (socket_id sid) {
            NETTY__TRACE("[node]", "{}: socket heartbeat timeout exceeded: #{}", _name, sid);
            schedule_reconnection(sid);
        });

        NETTY__TRACE("[node]", "node: {} (gateway={}, id={})"
            , _name, _is_gateway, node_id_traits::to_string(_id_rep));
    }

    node (node const &) = delete;
    node (node &&) = delete;
    node & operator = (node const &) = delete;
    node & operator = (node &&) = delete;

    ~node () {}

public:
    node_id id () const noexcept
    {
        return node_id_traits::cast(_id_rep);
    }

    node_id_rep const & id_rep () const noexcept
    {
        return _id_rep;
    }

    void set_name (std::string const & name)
    {
        _name = name;
    }

    std::string const & name () const noexcept
    {
        return _name;
    }

    bool is_gateway () const noexcept
    {
        return _is_gateway;
    }

    /**
     * Sets the node index.
     *
     * @details Make sense when used inside node_pool.
     */
    void set_index (node_index_t index) noexcept
    {
        _index = index;
    }

    node_index_t index () const noexcept
    {
        return _index;
    }

    void add_listener (netty::socket4_addr const & listener_addr, error * perr = nullptr)
    {
        _listener_pool.add(listener_addr, perr);
    }

    bool connect_host (netty::socket4_addr remote_saddr, bool behind_nat = false)
    {
        netty::error err;
        auto rs = _connecting_pool.connect(remote_saddr);

        if (rs == netty::conn_status::failure)
            return false;

        if (behind_nat)
            _behind_nat.insert(remote_saddr);

        return true;
    }

    bool connect_host (netty::socket4_addr remote_saddr, netty::inet4_addr local_addr, bool behind_nat)
    {
        netty::error err;
        auto rs = _connecting_pool.connect(remote_saddr, local_addr);

        if (rs == netty::conn_status::failure)
            return false;

        if (behind_nat)
            _behind_nat.insert(remote_saddr);

        return true;
    }

    void listen (int backlog = 50)
    {
        _listener_pool.listen(backlog);
    }

    void enqueue (node_id_rep const & id_rep, int priority, bool force_checksum
        , char const * data, std::size_t len)
    {
        auto sid_ptr = _channels.locate_writer(id_rep);

        if (sid_ptr != nullptr) {
            auto out = serializer_traits::make_serializer();
            ddata_packet pkt {force_checksum};
            pkt.serialize(out, data, len);
            enqueue_private(*sid_ptr, priority, out.take());
        } else {
            _callbacks->on_error(tr::f_("channel for send message not found: {}"
                , node_id_traits::to_string(id_rep)));
        }
    }

    void enqueue (node_id_rep const & id_rep, int priority, bool force_checksum
        , std::vector<char> && data)
    {
        auto sid_ptr = _channels.locate_writer(id_rep);

        if (sid_ptr != nullptr) {
            auto out = serializer_traits::make_serializer();
            ddata_packet pkt {force_checksum};
            pkt.serialize(out, std::move(data));
            enqueue_private(*sid_ptr, priority, out.take());
        } else {
            _callbacks->on_error(tr::f_("channel for send message not found: {}"
                , node_id_traits::to_string(id_rep)));
        }
    }

    void enqueue (node_id_rep const & id_rep, int priority, char const * data, std::size_t len)
    {
        this->enqueue(id_rep, priority, false, data, len);
    }

    void enqueue (node_id_rep const & id_rep, int priority, std::vector<char> && data)
    {
        this->enqueue(id_rep, priority, false, std::move(data));
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step ()
    {
        unsigned int result = 0;

        result += _listener_pool.step();
        result += _connecting_pool.step();
        result += _writer_pool.step();
        result += _reader_pool.step();

        result += _handshake_processor.step();
        result += _heartbeat_processor.step();

        // Remove trash
        _connecting_pool.apply_remove();
        _listener_pool.apply_remove();
        _reader_pool.apply_remove();
        _writer_pool.apply_remove();
        _socket_pool.apply_remove(); // Must be last in the removing sequence

        return result;
    }

    /**
     * Checks if this channel has direct writer to specified node by @a id.
     */
    bool has_writer (node_id_rep const & id_rep) const
    {
        return _channels.locate_writer(id_rep) != nullptr;
    }

    /**
     * Sets frame size for exchange with node specified by identifier @a id.
     */
    void set_frame_size (node_id_rep const & id, std::uint16_t frame_size)
    {
        auto sid_ptr = _channels.locate(id);

        if (sid_ptr != nullptr)
            _writer_pool.ensure(*sid_ptr, frame_size);
    }

public: // static
    static constexpr int priority_count () noexcept
    {
        return writer_pool_type::priority_count();
    }

private:
    void schedule_reconnection (socket4_addr saddr)
    {
        if (!reconnection_policy::supported())
            return;

        auto reconnecting = true;
        auto pos = _reconn_policies.find(saddr);

        if (pos == _reconn_policies.end()) {
            pos = _reconn_policies.emplace(saddr, reconnection_policy{}).first;
        } else {
            if (!pos->second.required()) {
                _reconn_policies.erase(pos);
                reconnecting = false;
                NETTY__TRACE("[node]", "{}: stopped reconnection to: {}", _name, to_string(saddr));
            }
        }

        if (reconnecting) {
            auto reconn_timeout = pos->second.fetch_timeout();
            NETTY__TRACE("[node]", "{}: reconnecting to: {} after {}", _name, to_string(saddr)
                , reconn_timeout);
            _connecting_pool.connect_timeout(reconn_timeout, saddr);
        }
    }

    void schedule_reconnection (socket_id sid)
    {
        if (reconnection_policy::supported()) {
            bool is_accepted = false;
            auto psock = _socket_pool.locate(sid, & is_accepted);
            auto reconnecting = !is_accepted;

            PFS__TERMINATE(psock != nullptr, "Fix meshnet::node algorithm");

            if (reconnecting)
                schedule_reconnection(psock->saddr());
        }

        _channels.close_channel(sid);
    }

    void process_alive_info (socket_id sid, alive_info const & ainfo)
    {
        auto id_ptr = _channels.locate_reader(sid);

        if (id_ptr != nullptr)
            _callbacks->on_alive_received(*id_ptr, _index, ainfo);
    }

    void process_unreachable_info (socket_id sid, unreachable_info const & uinfo)
    {
        auto id_ptr = _channels.locate_reader(sid);

        if (id_ptr != nullptr)
            _callbacks->on_unreachable_received(*id_ptr, _index, uinfo);
    }

    void process_route_info (socket_id sid, bool is_response, route_info const & rinfo)
    {
        auto id_ptr = _channels.locate_reader(sid);

        if (id_ptr != nullptr)
            _callbacks->on_route_received(*id_ptr, _index, is_response, rinfo);
    }

    void process_message_received (socket_id sid, int priority, std::vector<char> && bytes)
    {
        auto id_ptr = _channels.locate_reader(sid);

        if (id_ptr != nullptr)
            _callbacks->on_domestic_message_received(*id_ptr, priority, std::move(bytes));
    }

    void process_message_received (socket_id sid, int priority, node_id_rep const & sender_id
        , node_id_rep const & receiver_id, std::vector<char> && bytes)
    {
        auto id_ptr = _channels.locate_reader(sid);

        if (id_ptr != nullptr) {
            _callbacks->on_global_message_received(*id_ptr
                , priority
                , sender_id
                , receiver_id
                , std::move(bytes));
        }
    }

    void forward_global_message (int priority, node_id_rep const & sender_id
        , node_id_rep const & receiver_id, std::vector<char> && packet)
    {
        _callbacks->forward_global_message(priority, sender_id, receiver_id, std::move(packet));
    }

    HandshakeProcessor<node> & handshake_processor ()
    {
        return _handshake_processor;
    }

    HeartbeatProcessor<node> & heartbeat_processor ()
    {
        return _heartbeat_processor;
    }

public: // Below methods are for internal use only
    void enqueue_private (socket_id sid, int priority, char const * data, std::size_t len)
    {
        _writer_pool.enqueue(sid, priority, data, len);
    }

    void enqueue_private (socket_id sid, int priority, std::vector<char> && data)
    {
        _writer_pool.enqueue(sid, priority, std::move(data));
    }

    void enqueue_broadcast_private (int priority, char const * data, std::size_t len)
    {
        _writer_pool.enqueue_broadcast(priority, data, len);
    }

public: // node_interface
    template <class Node>
    class node_interface_impl: public node_interface<node_id_traits>
        , protected Node
    {
    public:
        template <typename ...Args>
        node_interface_impl (Args &&... args)
            : Node(std::forward<Args>(args)...)
        {}

        virtual ~node_interface_impl () {}

    public:
        node_id id () const noexcept override
        {
            return Node::id();
        }

        node_id_rep id_rep () const noexcept override
        {
            return Node::id_rep();
        }

        std::string name () const noexcept override
        {
            return Node::name();
        }

        void set_index (node_index_t index) noexcept override
        {
            Node::set_index(index);
        }

        node_index_t index () const noexcept override
        {
            return Node::index();
        }

        void add_listener (netty::socket4_addr const & listener_addr, error * perr = nullptr) override
        {
            Node::add_listener(listener_addr, perr);
        }

        bool connect_host (netty::socket4_addr remote_saddr, bool behind_nat) override
        {
            return Node::connect_host(remote_saddr, behind_nat);
        }

        bool connect_host (netty::socket4_addr remote_saddr, netty::inet4_addr local_addr, bool behind_nat) override
        {
            return Node::connect_host(remote_saddr, local_addr, behind_nat);
        }

        void listen (int backlog = 50) override
        {
            Node::listen(backlog);
        }

        void enqueue (node_id_rep const & id, int priority, bool force_checksum, char const * data
            , std::size_t len) override
        {
            Node::enqueue(id, priority, force_checksum, data, len);
        }

        void enqueue (node_id_rep const & id, int priority, bool force_checksum
            , std::vector<char> && data) override
        {
            Node::enqueue(id, priority, force_checksum, std::move(data));
        }

        bool has_writer (node_id_rep const & id_rep) const override
        {
            return Node::has_writer(id_rep);
        }

        unsigned int step () override
        {
            return Node::step();
        }

        void enqueue_packet (node_id_rep const & id_rep, int priority, std::vector<char> && data) override
        {
            auto sid_ptr = Node::_channels.locate_writer(id_rep);

            if (sid_ptr != nullptr) {
                Node::enqueue_private(*sid_ptr, priority, std::move(data));
            } else {
                Node::_callbacks->on_error(tr::f_("channel for send packet not found: {}"
                    , node_id_traits::to_string(id_rep)));
            }
        }

        void enqueue_packet (node_id_rep const & id_rep, int priority, char const * data, std::size_t len) override
        {
            auto sid_ptr = Node::_channels.locate_writer(id_rep);

            if (sid_ptr != nullptr) {
                Node::enqueue_private(*sid_ptr, priority, data, len);
            } else {
                Node::_callbacks->on_error(tr::f_("channel for send packet not found: {}"
                    , node_id_traits::to_string(id_rep)));
            }
        }

        void enqueue_broadcast_packet (int priority, char const * data, std::size_t len) override
        {
            Node::enqueue_broadcast_private(priority, data, len);
        }
    };

    template <typename ...Args>
    static std::unique_ptr<node_interface<node_id_traits>>
    make_interface (Args &&... args)
    {
        return std::unique_ptr<node_interface<node_id_traits>>(
            new node_interface_impl<node_class>(std::forward<Args>(args)...));
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
