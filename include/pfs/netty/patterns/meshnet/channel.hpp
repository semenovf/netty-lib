////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "handshake_result.hpp"
#include "channel_interface.hpp"
#include "unordered_bimap.hpp"
#include "../../conn_status.hpp"
#include "../../connection_refused_reason.hpp"
#include "../../error.hpp"
#include "../../namespace.hpp"
#include "../../socket4_addr.hpp"
#include "../../connecting_pool.hpp"
#include "../../listener_pool.hpp"
#include "../../reader_pool.hpp"
#include "../../socket_pool.hpp"
#include "../../writer_pool.hpp"
#include <pfs/countdown_timer.hpp>
#include <pfs/i18n.hpp>
#include <chrono>
#include <memory>
#include <thread>
#include <unordered_map>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdTraits
    , typename Listener
    , typename Socket
    , typename ConnectingPoller
    , typename ListenerPoller
    , typename ReaderPoller
    , typename WriterPoller
    , typename WriterQueue
    , typename SerializerTraits
    , typename ReconnectionPolicy
    , template <typename> class HandshakeProcessor
    , template <typename> class HeartbeatProcessor
    , template <typename> class MessageSender
    , template <typename> class InputProcessor
    , template <typename> class CallbackSuite
    , typename Loggable>
class channel: public Loggable
{
    friend class HandshakeProcessor<channel>;
    friend class HeartbeatProcessor<channel>;
    friend /*class */InputProcessor<channel>;

    using channel_class = channel<NodeIdTraits
        , Listener
        , Socket
        , ConnectingPoller
        , ListenerPoller
        , ReaderPoller
        , WriterPoller
        , WriterQueue
        , SerializerTraits
        , ReconnectionPolicy
        , HandshakeProcessor
        , HeartbeatProcessor
        , MessageSender
        , InputProcessor
        , CallbackSuite
        , Loggable>;

    using listener_type = Listener;
    using socket_type = Socket;
    using socket_pool_type = netty::socket_pool<socket_type>;
    using connecting_pool_type = netty::connecting_pool<socket_type, ConnectingPoller>;
    using listener_pool_type = netty::listener_pool<listener_type, socket_type, ListenerPoller>;
    using reader_pool_type = netty::reader_pool<socket_type, ReaderPoller>;
    using writer_pool_type = netty::writer_pool<socket_type, WriterPoller, WriterQueue>;
    using listener_id = typename listener_type::listener_id;
    using reconnection_policy = ReconnectionPolicy;

public:
    using node_id_traits = NodeIdTraits;
    using node_id = typename NodeIdTraits::node_id;
    using socket_id = typename socket_type::socket_id;
    using serializer_traits = SerializerTraits;
    using callback_suite = CallbackSuite<node_id>;

private:
    node_id              _id;
    listener_pool_type   _listener_pool;
    connecting_pool_type _connecting_pool;
    reader_pool_type     _reader_pool;
    writer_pool_type     _writer_pool;
    socket_pool_type     _socket_pool;

    // True if the channel is behind NAT
    bool _behind_nat {false};

    HandshakeProcessor<channel> _handshake_processor;
    HeartbeatProcessor<channel> _heartbeat_processor;
    MessageSender<channel> _message_sender;
    InputProcessor<channel> _input_processor;
    std::shared_ptr<callback_suite> _callbacks;

    unordered_bimap<socket_id, node_id> _readers;
    unordered_bimap<socket_id, node_id> _writers;

public:
    channel (node_id id, bool behind_nat, std::shared_ptr<callback_suite> callbacks)
        : Loggable()
        , _id(id)
        , _behind_nat(behind_nat)
        , _handshake_processor(*this)
        , _heartbeat_processor(*this)
        , _message_sender(*this)
        , _input_processor(*this)
        , _callbacks(callbacks)
    {
        _listener_pool.on_failure([this] (netty::error const & err) {
            this->log_error(tr::f_("listener pool failure: {}", err.what()));
        }).on_accepted([this] (socket_type && sock) {
            this->log_debug(tr::f_("socket accepted: #{}: {}", sock.id(), to_string(sock.saddr())));
            _input_processor.add(sock.id());
            _reader_pool.add(sock.id());
            _socket_pool.add_accepted(std::move(sock));
        });

        _connecting_pool.on_failure([this] (netty::error const & err) {
            this->log_error(tr::f_("connecting pool failure: {}", err.what()));
        }).on_connected([this] (socket_type && sock) {
            this->log_debug(tr::f_("socket connected: #{}: {}", sock.id(), to_string(sock.saddr())));
            _handshake_processor.start(sock.id());
            _input_processor.add(sock.id());
            _reader_pool.add(sock.id());
            _socket_pool.add_connected(std::move(sock));
        }).on_connection_refused ([this] (socket_id sid, netty::socket4_addr saddr
                , netty::connection_refused_reason reason) {
            this->log_error(tr::f_("connection refused for socket: #{}: {}: reason: {}"
                ", reconnecting", sid, to_string(saddr), to_string(reason)));

            if (reconnection_policy::timeout() > std::chrono::seconds{0})
                _connecting_pool.connect_timeout(reconnection_policy::timeout(), saddr);
        });

        _reader_pool.on_failure([this] (socket_id sid, netty::error const & err) {
            this->log_error(tr::f_("read from socket failure: #{}: {}", sid, err.what()));
            close_socket(sid);
        }).on_disconnected([this] (socket_id sid) {
            this->log_debug(tr::f_("socket disconnected: #{}", sid));
            schedule_reconnection(sid);
            close_socket(sid);
        }).on_data_ready([this] (socket_id sid, std::vector<char> && data) {
            _input_processor.process_input(sid, std::move(data));
        }).on_locate_socket([this] (socket_id sid) {
            return _socket_pool.locate(sid);
        });

        _writer_pool.on_failure([this] (socket_id sid, netty::error const & err) {
            this->log_error(tr::f_("write to socket failure: #{}: {}", sid, err.what()));
            schedule_reconnection(sid);
            close_socket(sid);
        }).on_bytes_written([this] (socket_id sid, std::uint64_t n) {
            auto id_ptr = _writers.locate_by_first(sid);

            if (id_ptr != nullptr)
                _callbacks->on_bytes_written(*id_ptr, n);
        }).on_locate_socket([this] (socket_id sid) {
            return _socket_pool.locate(sid);
        });

        _handshake_processor.on_failure([this] (socket_id sid, std::string const & errstr) {
            this->log_error(errstr);
            close_socket(sid);
        }).on_expired([this] (socket_id sid) {
            this->log_warn(tr::f_("handshake expired for socket: #{}", sid));
            close_socket(sid);
        }).on_completed([this] (node_id id, socket_id sid, handshake_result_enum status) {
            switch (status) {
                case handshake_result_enum::unusable:
                    this->log_debug(tr::f_("handshake state changed: socket #{} excluded for channel: {}"
                        , sid, node_id_traits::stringify(id)));
                    close_socket(sid);
                    break;

                case handshake_result_enum::reader:
                    this->log_debug(tr::f_("handshake state changed: socket #{} is reader for channel: {}"
                        , sid, node_id_traits::stringify(id)));
                    _readers.insert(sid, id);
                    _heartbeat_processor.add(sid);

                    // If the writer already set, full virtual connection established with the
                    // neighbor channel.
                    if (_writers.locate_by_second(id) != nullptr)
                        _callbacks->on_channel_established(id);

                    break;

                case handshake_result_enum::writer:
                    this->log_debug(tr::f_("handshake state changed: socket #{} is writer for node: {}"
                        , sid, node_id_traits::stringify(id)));
                    _writers.insert(sid, id);
                    _heartbeat_processor.add(sid);

                    // If the reader already set, channel established with the neighbor node.
                    if (_readers.locate_by_second(id) != nullptr)
                        _callbacks->on_channel_established(id);

                    break;

                default:
                    PFS__TERMINATE(false, "Fix meshnet::channel algorithm");
                    break;
            }
        });

        _heartbeat_processor.on_expired ([this] (socket_id sid) {
            this->log_warn(tr::f_("socket heartbeat timeout exceeded: #{}", sid));
            schedule_reconnection(sid);
            close_socket(sid);
        });

        this->log_debug(tr::f_("Node: {}", node_id_traits::stringify(_id)));
    }

    channel (channel const &) = delete;
    channel (channel &&) = delete;
    channel & operator = (channel const &) = delete;
    channel & operator = (channel &&) = delete;

    ~channel () {}

public:
    node_id id () const noexcept
    {
        return _id;
    }

    bool is_behind_nat () const noexcept
    {
        return _behind_nat;
    }

    void add_listener (netty::socket4_addr const & listener_addr, error * perr = nullptr)
    {
        _listener_pool.add(listener_addr, perr);
    }

    bool connect_host (netty::socket4_addr remote_saddr)
    {
        netty::error err;
        auto rs = _connecting_pool.connect(remote_saddr);
        return rs != netty::conn_status::failure;
    }

    bool connect_host (netty::socket4_addr remote_saddr, netty::inet4_addr local_addr)
    {
        netty::error err;
        auto rs = _connecting_pool.connect(remote_saddr, local_addr);
        return rs != netty::conn_status::failure;
    }

    void listen (int backlog = 50)
    {
        _listener_pool.listen(backlog);
    }

    void enqueue (node_id id, int priority, bool force_checksum, char const * data, std::size_t len)
    {
        auto sid_ptr = _writers.locate_by_second(id);

        if (sid_ptr != nullptr) {
            _message_sender.enqueue(*sid_ptr, priority, force_checksum, data, len);
        } else {
            this->log_error(tr::f_("channel for send message not found: {}", node_id_traits::stringify(id)));
        }
    }

    void enqueue (node_id id, int priority, bool force_checksum, std::vector<char> && data)
    {
        auto sid_ptr = _writers.locate_by_second(id);

        if (sid_ptr != nullptr) {
            _message_sender.send(*sid_ptr, priority, force_checksum, std::move(data));
        } else {
            this->log_error(tr::f_("channel for send message not found: {}", node_id_traits::stringify(id)));
        }
    }

    void enqueue (node_id id, int priority, char const * data, std::size_t len)
    {
        this->enqueue(id, priority, false, data, len);
    }

    void enqueue (node_id id, int priority, std::vector<char> && data)
    {
        this->enqueue(id, priority, false, std::move(data));
    }

    void step (std::chrono::milliseconds millis = std::chrono::milliseconds{0})
    {
        pfs::countdown_timer<std::milli> countdown_timer {millis};

        _listener_pool.step();
        _connecting_pool.step();
        _writer_pool.step();

        millis = countdown_timer.remain();

        _reader_pool.step(millis);
        _handshake_processor.step();
        _heartbeat_processor.step();

        // Remove trash
        _connecting_pool.apply_remove();
        _listener_pool.apply_remove();
        _reader_pool.apply_remove();
        _writer_pool.apply_remove();
        _socket_pool.apply_remove(); // Must be last in the removing sequence

        std::this_thread::sleep_for(countdown_timer.remain());
    }

    /**
     * Checks if this channel has direct writer to specified node by @a id.
     */
    bool has_writer (node_id id) const
    {
        return _writers.locate_by_second(id) != nullptr;
    }

    /**
     * Sets frame size for exchange with node specified by identifier @a id.
     */
    void set_frame_size (node_id id, std::uint16_t frame_size)
    {
        auto sid_ptr = _writers.locate_by_second(id);

        if (sid_ptr != nullptr)
            _writer_pool.ensure(*sid_ptr, frame_size);
    }

public: // static
    static constexpr int priority_count () noexcept
    {
        return writer_pool_type::priority_count();
    }

private:
    // Acceptable values for level: 0 or 1
    void close_socket (socket_id sid, int level = 0)
    {
        _handshake_processor.cancel(sid);
        _heartbeat_processor.remove(sid);
        _input_processor.remove(sid);
        _reader_pool.remove_later(sid);
        _writer_pool.remove_later(sid);
        _socket_pool.remove_later(sid);

        if (level == 0)
            close_channel(sid, level);
    }

    // Closes channel associated with socket identifier.
    // One socket may be reader and writer simultaneously, or reader and writer may be represented
    // by two different sockets.
    // Acceptable values for level: 0 or 1
    void close_channel (socket_id sid, int level)
    {
        auto r_id_ptr = _readers.locate_by_first(sid);
        auto w_id_ptr = _writers.locate_by_first(sid);

        // Channel not established (full and partially)
        if (r_id_ptr == nullptr && w_id_ptr == nullptr)
            return;

        // Channel already established and one socket are reader and writer simultaneously
        if (r_id_ptr != nullptr && w_id_ptr != nullptr) {
            auto id = *w_id_ptr;
            PFS__ASSERT(id == *r_id_ptr, "Fix meshnet::channel algorithm");
            _readers.erase_by_second(id);
            _writers.erase_by_second(id);
            _callbacks->on_channel_destroyed(id);
            return;
        }

        if (r_id_ptr == nullptr) {
            PFS__ASSERT(w_id_ptr != nullptr, "Fix meshnet::channel algorithm");

            auto id = *w_id_ptr;
            auto r_sid_ptr = _readers.locate_by_second(id);

            // Channel already established and reader and writer represented by two different sockets
            if (r_sid_ptr != nullptr) {
                close_socket(*r_sid_ptr, ++level);
                _readers.erase_by_first(*r_sid_ptr);
                _writers.erase_by_second(id);
                _callbacks->on_channel_destroyed(id);
            } else {
                _writers.erase_by_second(id);
            }

            return;
        }

        if (w_id_ptr == nullptr) {
            PFS__ASSERT(r_id_ptr != nullptr, "Fix meshnet::channel algorithm");

            auto id = *r_id_ptr;
            auto w_sid_ptr = _writers.locate_by_second(id);

            // Channel already established and reader and writer represented by two different sockets
            if (w_sid_ptr != nullptr) {
                close_socket(*w_sid_ptr, ++level);
                _readers.erase_by_second(id);
                _writers.erase_by_first(*w_sid_ptr);
                _callbacks->on_channel_destroyed(id);
            } else {
                _readers.erase_by_second(id);
            }

            return;
        }

        // Must be unreachable
        PFS__ASSERT(false, "Fix meshnet::channel algorithm");
    }

    void schedule_reconnection (socket_id sid)
    {
        if (reconnection_policy::timeout() > std::chrono::seconds{0}) {
            bool is_accepted = false;
            auto psock = _socket_pool.locate(sid, & is_accepted);
            auto reconnecting = !is_accepted;

            PFS__TERMINATE(psock != nullptr, "Fix meshnet::channel algorithm");

            if (reconnecting)
                _connecting_pool.connect_timeout(reconnection_policy::timeout(), psock->saddr());
        }
    }

    void process_route_received (socket_id sid, bool is_response, std::vector<std::pair<std::uint64_t, std::uint64_t>> && route)
    {
        _callbacks->on_route_received(sid, is_response, std::move(route));
    }

    void process_message_received (socket_id sid, std::vector<char> && bytes)
    {
        auto id_ptr = _readers.locate_by_first(sid);

        if (id_ptr != nullptr)
            _callbacks->on_message_received(*id_ptr, std::move(bytes));
    }

    void process_message_received (socket_id sid
        , std::pair<std::uint64_t, std::uint64_t> sender_id
        , std::pair<std::uint64_t, std::uint64_t> receiver_id
        , std::vector<char> && bytes)
    {
        auto id_ptr = _readers.locate_by_first(sid);

        if (id_ptr != nullptr) {
            _callbacks.on_foreign_message_received(*id_ptr
                , std::move(sender_id)
                , std::move(receiver_id)
                , std::move(bytes));
        }
    }

    HandshakeProcessor<channel> & handshake_processor ()
    {
        return _handshake_processor;
    }

    HeartbeatProcessor<channel> & heartbeat_processor ()
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

public: // channel_interface
    template <class Node>
    class channel_interface_impl: public channel_interface<NodeIdTraits>
        , public Node
    {
    public:
        template <typename ...Args>
        channel_interface_impl (Args &&... args)
            : Node(std::forward<Args>(args)...)
        {}

        virtual ~channel_interface_impl () {}

    public:
        void add_listener (netty::socket4_addr const & listener_addr, error * perr = nullptr) override
        {
            Node::add_listener(listener_addr, perr);
        }

        bool connect_host (netty::socket4_addr remote_saddr) override
        {
            return Node::connect_host(remote_saddr);
        }

        bool connect_host (netty::socket4_addr remote_saddr, netty::inet4_addr local_addr) override
        {
            return Node::connect_host(remote_saddr, local_addr);
        }

        void listen (int backlog = 50) override
        {
            Node::listen(backlog);
        }

        void enqueue (node_id id, int priority, bool force_checksum, char const * data, std::size_t len) override
        {
            Node::enqueue(id, priority, force_checksum, data, len);
        }

        void step (std::chrono::milliseconds limit) override
        {
            Node::step(limit);
        }
    };

    template <typename ...Args>
    static std::unique_ptr<channel_interface<NodeIdTraits>>
    make_interface (Args &&... args)
    {
        return std::unique_ptr<channel_interface<NodeIdTraits>>(
            new channel_interface_impl<channel_class>(std::forward<Args>(args)...));
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
