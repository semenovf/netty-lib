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
#include <pfs/countdown_timer.hpp>
#include <pfs/i18n.hpp>
#include <pfs/netty/conn_status.hpp>
#include <pfs/netty/connection_refused_reason.hpp>
#include <pfs/netty/error.hpp>
#include <pfs/netty/namespace.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <pfs/netty/connecting_pool.hpp>
#include <pfs/netty/listener_pool.hpp>
#include <pfs/netty/reader_pool.hpp>
#include <pfs/netty/socket_pool.hpp>
#include <pfs/netty/writer_pool.hpp>
#include <thread>
#include <unordered_map>

#include <pfs/log.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdintifierTraits
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
    , template <typename> class InputProcessor
    , template <typename> class CallbackSuite
    , typename Loggable>
class node: public Loggable
{
    friend class HandshakeProcessor<node>;
    friend class HeartbeatProcessor<node>;
    friend class InputProcessor<node>;

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
    using node_idintifier_traits = NodeIdintifierTraits;
    using node_id = typename NodeIdintifierTraits::node_id;
    using socket_id = typename socket_type::socket_id;
    using serializer_traits = SerializerTraits;
    using callback_suite = CallbackSuite<node>;

private:
    node_id              _id;
    listener_pool_type   _listener_pool;
    connecting_pool_type _connecting_pool;
    reader_pool_type     _reader_pool;
    writer_pool_type     _writer_pool;
    socket_pool_type     _socket_pool;

    // True if the node is behind NAT
    bool _behind_nat {false};

    // std::chrono::seconds _reconnection_timeout {5};
    HandshakeProcessor<node> _handshake_processor;
    HeartbeatProcessor<node> _heartbeat_processor;
    InputProcessor<node> _input_processor;
    callback_suite _callbacks;

    std::unordered_map<socket_id, node_id> _readers;
    std::unordered_map<node_id, socket_id> _writers;

public:
    node (node_id id, bool behind_nat, callback_suite && callbacks)
        : Loggable()
        , _id(id)
        , _behind_nat(behind_nat)
        , _handshake_processor(*this)
        , _heartbeat_processor(*this)
        , _input_processor(*this)
        , _callbacks(std::move(callbacks))
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
            this->log_error(tr::f_("read from socket failure: {}: {}", sid, err.what()));
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
        }).on_bytes_written([] (socket_id sid, std::uint64_t n) {
            // FIXME Use this callback to collect statistics
            // LOGD("***", "bytes written: id={}: {}", sid, n);
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
                    this->log_debug(tr::f_("handshake complete: socket #{} excluded for node: {}"
                        , sid, node_idintifier_traits::stringify(id)));
                    close_socket(sid);
                    break;

                case handshake_result_enum::reader:
                    this->log_debug(tr::f_("handshake complete: socket #{} is reader for node: {}"
                        , sid, node_idintifier_traits::stringify(id)));
                    _readers[sid] = id;
                    _heartbeat_processor.add(sid);

                    // If the writer already set, full virtual connection established with the
                    // neighbor node.
                    if (find_writer(id) != _writers.end())
                        _callbacks.on_node_connected(id);

                    break;

                case handshake_result_enum::writer:
                    this->log_debug(tr::f_("handshake complete: socket #{} is writer for node: {}"
                        , sid, node_idintifier_traits::stringify(id)));
                    _writers[id] = sid;
                    _heartbeat_processor.add(sid);

                    // If the reader already set, channel established with the neighbor node.
                    if (find_reader(id) != _readers.end())
                        _callbacks.on_node_connected(id);

                    break;

                default:
                    PFS__TERMINATE(false, "Fix meshnet::node algorithm");
                    break;
            }
        });

        _heartbeat_processor.on_expired ([this] (socket_id sid) {
            this->log_warn(tr::f_("socket heartbeat timeout exceeded: #{}", sid));
            schedule_reconnection(sid);
            close_socket(sid);
        });

        this->log_debug(tr::f_("Node: {}", node_idintifier_traits::stringify(_id)));
    }

    ~node () {}

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
        return rs == netty::conn_status::failure;
    }

    bool connect_host (netty::socket4_addr remote_saddr, netty::inet4_addr local_addr)
    {
        netty::error err;
        auto rs = _connecting_pool.connect(remote_saddr, local_addr);
        return rs == netty::conn_status::failure;
    }

    void listen (int backlog = 50)
    {
        _listener_pool.listen(backlog);
    }

    void send (node_id id, int priority, char const * data, std::size_t len)
    {
        auto pos = _writers.find(id);

        if (pos != _writers.end()) {
            send_private(pos->second, priority, data, len);
        } else {
            this->log_error(tr::f_("node for send message not found: {}", node_idintifier_traits::stringify(id)));
        }
    }

    void send (node_id id, int priority, std::vector<char> && data)
    {
        auto pos = _writers.find(id);

        if (pos != _writers.end()) {
            send_private(pos->second, priority, std::move(data));
        } else {
            this->log_error(tr::f_("node for send message not found: {}", node_idintifier_traits::stringify(id)));
        }
    }

    void step (std::chrono::milliseconds millis)
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

public: // static
    static constexpr int priority_count () noexcept
    {
        return writer_pool_type::priority_count();
    }

private:
    typename std::unordered_map<socket_id, node_id>::iterator find_reader (socket_id sid)
    {
        return _readers.find(sid);
    }

    typename std::unordered_map<socket_id, node_id>::iterator find_reader (node_id id)
    {
        for (auto pos = _readers.begin(); pos != _readers.end(); ++pos)
            if (pos->second == id)
                return pos;
        return _readers.end();
    }

    typename std::unordered_map<node_id, socket_id>::iterator find_writer (node_id id)
    {
        return _writers.find(id);
    }

    typename std::unordered_map<node_id, socket_id>::iterator find_writer (socket_id sid)
    {
        for (auto pos = _writers.begin(); pos != _writers.end(); ++pos)
            if (pos->second == sid)
                return pos;

        return _writers.end();
    }

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

    // TODO More optimized data structures and algorithms are needed to track the lifecycle of node
    // connections.
    //
    // Closes channel associated with socket identifier.
    // One socket may be reader and writer simultaneously, or reader and writer may be represented
    // by two different sockets.
    // Acceptable values for level: 0 or 1
    void close_channel (socket_id sid, int level)
    {
        auto rpos = find_reader(sid);
        auto wpos = find_writer(sid);

        // Channel not established (full and partially)
        if (rpos == _readers.end() && wpos == _writers.end())
            return;

        // Channel already established and one socket are reader and writer simultaneously
        if (rpos != _readers.end() && wpos != _writers.end()) {
            auto id = wpos->first;
            PFS__ASSERT(id == rpos->second, "Fix meshnet::node algorithm");
            _readers.erase(rpos);
            _writers.erase(wpos);
            _callbacks.on_node_disconnected(id);
            return;
        }

        if (rpos == _readers.end()) {
            PFS__ASSERT(wpos != _writers.end(), "Fix meshnet::node algorithm");

            rpos = find_reader(wpos->first); // find by node ID

            // Channel already established and reader and writer represented by two different sockets
            if (rpos != _readers.end()) {
                auto id = wpos->first;
                close_socket(rpos->first, ++level);
                _readers.erase(rpos);
                _writers.erase(wpos);
                _callbacks.on_node_disconnected(id);
            } else {
                _writers.erase(wpos);
            }

            return;
        }

        if (wpos == _writers.end()) {
            PFS__ASSERT(rpos != _readers.end(), "Fix meshnet::node algorithm");
            wpos = find_writer(rpos->second); // find by node ID

            // Channel already established and reader and writer represented by two different sockets
            if (wpos != _writers.end()) {
                auto id = rpos->second;
                close_socket(wpos->second, ++level);
                _readers.erase(rpos);
                _writers.erase(wpos);
                _callbacks.on_node_disconnected(id);
            } else {
                _readers.erase(rpos);
            }

            return;
        }

        // Must be unreachable
        PFS__ASSERT(false, "Fix meshnet::node algorithm");
    }

    void schedule_reconnection (socket_id sid)
    {
        if (reconnection_policy::timeout() > std::chrono::seconds{0}) {
            bool is_accepted = false;
            auto psock = _socket_pool.locate(sid, & is_accepted);
            auto reconnecting = !is_accepted;

            PFS__TERMINATE(psock != nullptr, "Fix meshnet::node algorithm");

            if (reconnecting)
                _connecting_pool.connect_timeout(reconnection_policy::timeout(), psock->saddr());
        }
    }

    void process_message_received (socket_id sid, std::vector<char> && bytes)
    {
        auto pos = _readers.find(sid);

        if (pos != _readers.end())
            _callbacks.on_message_received(pos->second, std::move(bytes));
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
    void send_private (socket_id sid, int priority, char const * data, std::size_t len)
    {
        _writer_pool.enqueue(sid, priority, data, len);
    }

    void send_private (socket_id sid, int priority, std::vector<char> && data)
    {
        _writer_pool.enqueue(sid, priority, std::move(data));
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
