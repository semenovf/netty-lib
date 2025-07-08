////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../callback.hpp"
#include "../../conn_status.hpp"
#include "../../connection_failure_reason.hpp"
#include "../../error.hpp"
#include "../../socket4_addr.hpp"
#include "../../connecting_pool.hpp"
#include "../../listener_pool.hpp"
#include "../../reader_pool.hpp"
#include "../../socket_pool.hpp"
#include "../../trace.hpp"
#include "../../writer_pool.hpp"
#include "alive_info.hpp"
#include "channel_map.hpp"
#include "node_index.hpp"
#include "node_interface.hpp"
#include "protocol.hpp"
#include "route_info.hpp"
#include "tag.hpp"
#include <pfs/i18n.hpp>
#include <pfs/log.hpp>
#include <pfs/optional.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

/**
 * Mesh network node.
 *
 * @details Usage example:
 *
 * @code
 *
 * // Typedef specialization.
 * using node_t = netty::patterns::meshnet::node<...>;
 *
 * node_t::node_id id = <Node-ID>;
 * bool is_gateway = <true | false>;
 *
 * // Define instance
 * node_t node { id, is_gateway };
 *
 * // Assign callbacks
 * node.on_error = ...;
 * node.on_channel_established = ...;
 * node.on_channel_destroyed = ...;
 * ...
 *
 * // Run node loop
 * while (! interrupted)
 *     node.step();
 *
 * @endcode
 */
template <typename NodeId
    , typename Socket
    , typename Listener
    , typename ConnectingPoller
    , typename ListenerPoller
    , typename ReaderPoller
    , typename WriterPoller
    , typename WriterQueue
    , typename RecursiveWriterMutex
    , typename SerializerTraits
    , typename ReconnectionPolicy
    , template <typename> class HandshakeController
    , template <typename> class HeartbeatController
    , template <typename> class InputController>
class node
{
    friend class HandshakeController<node>;
    friend class HeartbeatController<node>;
    friend /*class */InputController<node>;

private:
    using node_class = node<NodeId
        , Socket
        , Listener
        , ConnectingPoller
        , ListenerPoller
        , ReaderPoller
        , WriterPoller
        , WriterQueue
        , RecursiveWriterMutex
        , SerializerTraits
        , ReconnectionPolicy
        , HandshakeController
        , HeartbeatController
        , InputController>;

    using socket_type = Socket;
    using listener_type = Listener;
    using socket_pool_type = netty::socket_pool<socket_type>;
    using connecting_pool_type = netty::connecting_pool<socket_type, ConnectingPoller>;
    using listener_pool_type = netty::listener_pool<listener_type, socket_type, ListenerPoller>;
    using reader_pool_type = netty::reader_pool<socket_type, ReaderPoller>;
    using writer_pool_type = netty::writer_pool<socket_type, WriterPoller, WriterQueue>;
    using writer_mutex_type = RecursiveWriterMutex;
    using listener_id = typename listener_type::listener_id;
    using reconnection_policy = ReconnectionPolicy;

public:
    using socket_id = typename socket_type::socket_id;
    using serializer_traits = SerializerTraits;
    using node_id = NodeId;

private:
    using channel_collection_type = channel_map<node_id, socket_id>;

    struct host_info
    {
        socket4_addr remote_saddr;
        inet4_addr local_addr;
        pfs::optional<reconnection_policy> reconn_policy;
    };

    using host_cache_type = std::map<socket4_addr, host_info>;

private:
    // Unique node identifier
    node_id _id;

    channel_collection_type _channels;

    listener_pool_type   _listener_pool;
    connecting_pool_type _connecting_pool;
    reader_pool_type     _reader_pool;
    writer_pool_type     _writer_pool;
    socket_pool_type     _socket_pool;

    // True if the node is a part of gateway
    bool _is_gateway {false};

    HandshakeController<node> _handshake_controller;
    HeartbeatController<node> _heartbeat_controller;
    InputController<node> _input_controller;

    // std::map<socket4_addr, reconnection_policy> _reconn_policies;
    host_cache_type _hosts_cache;

    // Nodes for which the current node is behind NAT
    std::set<socket4_addr> _behind_nat;

    // Make sense when node is a part of node_pool.
    node_index_t _index {INVALID_NODE_INDEX};

    // Writer mutex to protect sending
    writer_mutex_type _writer_mtx;

private: // Callbacks
    callback_t<void (std::string const &)> _on_error
        = [] (std::string const & errstr) { LOGE(TAG, "{}", errstr); };

    callback_t<void (node_id, node_index_t, bool)> _on_channel_established = [] (node_id, node_index_t, bool) {};
    callback_t<void (node_id, node_index_t)> _on_channel_destroyed = [] (node_id, node_index_t) {};
    callback_t<void (node_index_t, socket4_addr, inet4_addr)> _on_reconnection_started;
    callback_t<void (node_index_t, socket4_addr, inet4_addr)> _on_reconnection_stopped;
    callback_t<void (node_id, node_index_t, socket4_addr)> _on_duplicate_id;
    callback_t<void (node_id, node_index_t, std::size_t)> _on_bytes_written;
    callback_t<void (node_id, node_index_t, alive_info<node_id> const &)> _on_alive_received;
    callback_t<void (node_id, node_index_t, unreachable_info<node_id> const &)> _on_unreachable_received;
    callback_t<void (node_id, node_index_t, bool, route_info<node_id> const &)> _on_route_received;
    callback_t<void (node_id, int, std::vector<char>)> _on_domestic_data_received;
    callback_t<void (node_id, int, node_id, node_id, std::vector<char>)> _on_global_data_received;
    callback_t<void (int, node_id, node_id, std::vector<char>)> _on_forward_global_packet;

public:
    node (node_id id, bool is_gateway = false)
        : _id(id)
        , _is_gateway(is_gateway)
        , _handshake_controller(this)
        , _heartbeat_controller(this)
        , _input_controller(this)
    {
        _channels.close_socket = [this] (socket_id sid)
        {
            close_socket(sid);
        };

        _listener_pool.on_failure = [this] (netty::error const & err)
        {
            _on_error(tr::f_("listener pool failure: {}", err.what()));
        };

        _listener_pool.on_accepted = [this] (socket_type && sock)
        {
            NETTY__TRACE(MESHNET_TAG, "socket accepted: #{}: {}", sock.id(), to_string(sock.saddr()));
            _input_controller.add(sock.id());
            _reader_pool.add(sock.id());
            _socket_pool.add_accepted(std::move(sock));
        };

        _connecting_pool.on_failure = [this] (netty::error const & err)
        {
            _on_error(tr::f_("connecting pool failure: {}", err.what()));
        };

        _connecting_pool.on_connected = [this] (socket_type && sock)
        {
            NETTY__TRACE(MESHNET_TAG, "socket connected: #{}: {}", sock.id(), to_string(sock.saddr()));

            bool behind_nat = false;

            if (_behind_nat.find(sock.saddr()) != _behind_nat.end())
                behind_nat = true;

            // Stop reconnection if needed
            //_reconn_policies.erase(sock.saddr());
            stop_reconnection(_hosts_cache.find(sock.saddr()));

            _handshake_controller.start(sock.id(), behind_nat);
            _input_controller.add(sock.id());
            _reader_pool.add(sock.id());
            _socket_pool.add_connected(std::move(sock));
        };

        _connecting_pool.on_connection_refused = [this] (netty::socket4_addr saddr
            , netty::connection_failure_reason reason)
        {
            _on_error(tr::f_("connection refused for socket: {}: reason: {}"
                , to_string(saddr), to_string(reason)));
            schedule_reconnection(saddr);
        };

        _reader_pool.on_failure = [this] (socket_id sid, netty::error const & err)
        {
            _on_error(tr::f_("read from socket failure: #{}: {}", sid, err.what()));
            schedule_reconnection(sid);
        };

        _reader_pool.on_disconnected = [this] (socket_id sid)
        {
            NETTY__TRACE(MESHNET_TAG, "reader socket disconnected: #{}", sid);
            schedule_reconnection(sid);
        };

        _reader_pool.on_data_ready = [this] (socket_id sid, std::vector<char> && data)
        {
            _input_controller.process_input(sid, std::move(data));
        };

        _reader_pool.locate_socket = [this] (socket_id sid)
        {
            return _socket_pool.locate(sid);
        };

        _writer_pool.on_failure = [this] (socket_id sid, netty::error const & err)
        {
            _on_error(tr::f_("write to socket failure: #{}: {}", sid, err.what()));
            schedule_reconnection(sid);
        };

        _writer_pool.on_disconnected = [this] (socket_id sid)
        {
            NETTY__TRACE(MESHNET_TAG, "writer socket disconnected: #{}", sid);
            schedule_reconnection(sid);
        };

        _writer_pool.on_bytes_written = [this] (socket_id sid, std::uint64_t n)
        {
            if (_on_bytes_written) {
                auto id_ptr = _channels.locate_writer(sid);

                if (id_ptr != nullptr)
                    _on_bytes_written(*id_ptr, _index, n);
            }
        };

        _writer_pool.locate_socket = [this] (socket_id sid)
        {
            return _socket_pool.locate(sid);
        };

        _handshake_controller.on_expired = [this] (socket_id sid)
        {
            NETTY__TRACE(MESHNET_TAG, "handshake expired for socket: #{}", sid);
            schedule_reconnection(sid);
        };

        _handshake_controller.enqueue_packet = [this] (socket_id sid, std::vector<char> && data)
        {
            enqueue_private(sid, 0, std::move(data));
        };

        _handshake_controller.on_completed = [this] (node_id id, socket_id reader_sid
            , socket_id writer_sid, bool is_gateway)
        {
            auto success = _channels.insert(id, reader_sid, writer_sid);

            PFS__THROW_UNEXPECTED(success, "Fix handshake algorithm");

            _heartbeat_controller.update(writer_sid);

            _on_channel_established(id, _index, is_gateway);
        };

        _handshake_controller.on_duplicate_id = [this] (node_id id, socket_id sid, bool force_closing)
        {
            auto psock = _socket_pool.locate(sid);
            PFS__THROW_UNEXPECTED(psock != nullptr, "Fix meshnet::node algorithm");
            _on_duplicate_id(id, _index, psock->saddr());

            if (force_closing)
                destroy_channel(sid);
        };

        _handshake_controller.on_discarded = [this] (node_id id, socket_id sid)
        {
            NETTY__TRACE(MESHNET_TAG, "socket discarded by handshaking with: {} (sid={})", to_string(id), sid);
            destroy_channel(sid);
        };

        _heartbeat_controller.on_expired = [this] (socket_id sid) {
            NETTY__TRACE(MESHNET_TAG, "socket heartbeat timeout exceeded: #{}", sid);
            schedule_reconnection(sid);
        };

        NETTY__TRACE(MESHNET_TAG, "node constructed (id={}, gateway={})", to_string(_id)
            , _is_gateway);
    }

    node (node const &) = delete;
    node (node &&) = delete;
    node & operator = (node const &) = delete;
    node & operator = (node &&) = delete;

    ~node ()
    {
        clear_channels();
        NETTY__TRACE(MESHNET_TAG, "node destroyed: {}", to_string(_id));
    }

public: // Set callbacks
    /**
     * Sets error callback.
     *
     * @details Callback @a f signature must match:
     *          void (std::string const &)
     */
    template <typename F>
    node & on_error (F && f)
    {
        _on_error = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when connection established with the remote node.
     *
     * @details Callback @a f signature must match:
     *          void (node_id id, node_index_t, bool is_gateway)
     */
    template <typename F>
    node & on_channel_established (F && f)
    {
        _on_channel_established = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when the channel is destroyed with the remote node.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, node_index_t)
     */
    template <typename F>
    node & on_channel_destroyed (F && f)
    {
        _on_channel_destroyed = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when reconnection to remote node started.
     *
     * @details Callback @a f signature must match:
     *          void (node_index_t index, socket4_addr remote_addr, inet4_addr local_addr)
     */
    template <typename F>
    node & on_reconnection_started (F && f)
    {
        _on_reconnection_started = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when reconnection to remote node stopped.
     *
     * @details Callback @a f signature must match:
     *          void (node_index_t index, socket4_addr remote_addr, inet4_addr local_addr)
     */
    template <typename F>
    node & on_reconnection_stopped (F && f)
    {
        _on_reconnection_stopped = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when a node with identical ID is detected.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, node_index_t, socket4_addr)
     */
    template <typename F>
    node & on_duplicate_id (F && f)
    {
        _on_duplicate_id = std::forward<F>(f);
        return *this;
    }

    /**
     * On alive info received.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, node_index_t, alive_info<node_id> const &)
     */
    template <typename F>
    node & on_alive_received (F && f)
    {
        _on_alive_received = std::forward<F>(f);
        return *this;
    }

    /**
     * On unreachable node received
     *
     * @details Callback @a f signature must match:
     *          void (node_id, node_index_t, unreachable_info<node_id> const &)
     */
    template <typename F>
    node & on_unreachable_received (F && f)
    {
        _on_unreachable_received = std::forward<F>(f);
        return *this;
    }

    /**
     * Notify when data actually sent (written into the socket).
     *
     * @details Callback @a f signature must match:
     *          void (node_id id, std::uint64_t bytes_written_size)
     */
    template <typename F>
    node & on_bytes_written (F && f)
    {
        _on_bytes_written = std::forward<F>(f);
        return *this;
    }

    /**
     * On intermediate route info received.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, node_index_t, bool is_response, route_info<node_id> const &)
     */
    template <typename F>
    node & on_route_received (F && f)
    {
        _on_route_received = std::forward<F>(f);
        return *this;
    }

    /**
     * On domestic message received.
     *
     * @details Callback @a f signature must match:
     *          void (node_id, int priority, std::vector<char> bytes/)
     */
    template <typename F>
    node & on_domestic_data_received (F && f)
    {
        _on_domestic_data_received = std::forward<F>(f);
        return *this;
    }

    /**
     * On global (intersubnet) message received
     *
     * @details Callback @a f signature must match:
     *          void (node_id last_transmitter, int priority, node_id sender
     *              , node_id receiver, std::vector<char> data)
     */
    template <typename F>
    node & on_global_data_received (F && f)
    {
        _on_global_data_received = std::forward<F>(f);
        return *this;
    }

    /**
     * On global (intersubnet) message received
     *
     * @details Callback @a f signature must match:
     *          void (int priority, node_id sender, node_id receiver, std::vector<char> data)
     */
    template <typename F>
    node & on_forward_global_packet (F && f)
    {
        _on_forward_global_packet = std::forward<F>(f);
        return *this;
    }

public:
    node_id id () const noexcept
    {
        return _id;
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

        cache_host(remote_saddr, inet4_addr{});

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

        cache_host(remote_saddr, local_addr);

        return true;
    }

    void listen (int backlog = 50)
    {
        _listener_pool.listen(backlog);
    }

    bool enqueue (node_id id, int priority, bool force_checksum, char const * data, std::size_t len)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        auto sid_ptr = _channels.locate_writer(id);

        if (sid_ptr != nullptr) {
            auto out = serializer_traits::make_serializer();
            ddata_packet pkt {force_checksum};
            pkt.serialize(out, data, len);
            enqueue_private(*sid_ptr, priority, out.take());
            return true;
        }

        _on_error(tr::f_("channel for send message not found: {}", to_string(id)));
        return false;
    }

    bool enqueue (node_id id, int priority, bool force_checksum, std::vector<char> data)
    {
        return enqueue(id, priority, force_checksum, data.data(), data.size());
    }

    bool enqueue (node_id id, int priority, char const * data, std::size_t len)
    {
        return this->enqueue(id, priority, false, data, len);
    }

    bool enqueue (node_id id, int priority, std::vector<char> && data)
    {
        return this->enqueue(id, priority, false, std::move(data));
    }

    /**
     * Enqueue serialized packet to send.
     */
    bool enqueue_packet (node_id id, int priority, std::vector<char> && data)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        auto sid_ptr = _channels.locate_writer(id);

        if (sid_ptr != nullptr) {
            enqueue_private(*sid_ptr, priority, std::move(data));
            return true;
        }

        _on_error(tr::f_("channel for send packet not found: {}", to_string(id)));
        return false;
    }

    /**
     * Enqueue serialized packet to send.
     */
    bool enqueue_packet (node_id id, int priority, char const * data, std::size_t len)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        auto sid_ptr = _channels.locate_writer(id);

        if (sid_ptr != nullptr) {
            enqueue_private(*sid_ptr, priority, data, len);
            return true;
        }

        _on_error(tr::f_("channel for send packet not found: {}", to_string(id)));
        return false;
    }

    /**
     * Enqueue serialized packet to broadcast send.
     */
    void enqueue_broadcast_packet (int priority, char const * data, std::size_t len)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        _channels.for_each_writer([this, priority, data, len] (node_id, socket_id sid) {
            enqueue_private(sid, priority, data, len);
        });
    }

    /**
     * Enqueue serialized packet to forward it excluding sender.
     */
    void enqueue_forward_packet (node_id sender_id, int priority, char const * data, std::size_t len)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};

        _channels.for_each_writer([this, sender_id, priority, data, len] (node_id id, socket_id sid) {
            if (id != sender_id)
                enqueue_private(sid, priority, data, len);
        });
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step ()
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};
        unsigned int result = 0;

        result += _listener_pool.step();
        result += _connecting_pool.step();
        result += _writer_pool.step();
        result += _reader_pool.step();

        result += _handshake_controller.step();
        result += _heartbeat_controller.step();

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
    bool has_writer (node_id id) const
    {
        return _channels.locate_writer(id) != nullptr;
    }

    /**
     * Sets frame size for exchange with node specified by identifier @a id.
     */
    void set_frame_size (node_id id, std::uint16_t frame_size)
    {
        auto sid_ptr = _channels.locate_writer(id);

        if (sid_ptr != nullptr)
            _writer_pool.set_frame_size(*sid_ptr, frame_size);
    }

    /**
     * Close all channels and clear channel collection.
     */
    void clear_channels ()
    {
        _channels.clear();
    }

public: // static
    static constexpr int priority_count () noexcept
    {
        return writer_pool_type::priority_count();
    }

private:
    void close_socket (socket_id sid)
    {
        if (_socket_pool.locate(sid) != nullptr) {
            _handshake_controller.cancel(sid);
            _heartbeat_controller.remove(sid);
            _input_controller.remove(sid);
            _reader_pool.remove_later(sid);
            _writer_pool.remove_later(sid);
            _socket_pool.remove_later(sid);
        }
    }

    void stop_reconnection (typename host_cache_type::iterator pos)
    {
        PFS__THROW_UNEXPECTED(pos != _hosts_cache.end(), "Fix meshnet::node algorithm");

        auto & h = pos->second; // host_info instance

        if (h.reconn_policy.has_value()) {
            if (_on_reconnection_stopped)
                _on_reconnection_stopped(_index, h.remote_saddr, h.local_addr);

            h.reconn_policy.reset();
        }
    }

    void cache_host (netty::socket4_addr remote_saddr, netty::inet4_addr local_addr)
    {
        auto pos = _hosts_cache.find(remote_saddr);

        if (pos == _hosts_cache.end()) {
            _hosts_cache.insert({remote_saddr, host_info{remote_saddr, local_addr}});
        } else {
            auto & h = pos->second; // host_info instance
            h.reconn_policy.reset();
        }
    }

    void schedule_reconnection (socket4_addr saddr)
    {
        if (!reconnection_policy::supported())
            return;

        auto reconnecting = true;

        auto pos = _hosts_cache.find(saddr);

        PFS__THROW_UNEXPECTED(pos != _hosts_cache.end(), "Fix meshnet::node algorithm");

        auto & h = pos->second; // host_info instance

        if (!h.reconn_policy.has_value()) {
            h.reconn_policy = reconnection_policy{};
        } else {
            if (!h.reconn_policy->required()) {
                reconnecting = false;
                stop_reconnection(pos);
            }
        }

        if (reconnecting) {
            auto reconn_timeout = h.reconn_policy->fetch_timeout();

            NETTY__TRACE(MESHNET_TAG, "reconnecting to: {} after {}", to_string(h.remote_saddr)
                , reconn_timeout);

            if (h.local_addr != inet4_addr{})
                _connecting_pool.connect_timeout(reconn_timeout, h.remote_saddr, h.local_addr);
            else
                _connecting_pool.connect_timeout(reconn_timeout, h.remote_saddr);

            if (_on_reconnection_started)
                _on_reconnection_started(_index, h.remote_saddr, h.local_addr);
        }
    }

    void schedule_reconnection (socket_id sid)
    {
        if (reconnection_policy::supported()) {
            bool is_accepted = false;
            auto psock = _socket_pool.locate(sid, & is_accepted);

            if (psock != nullptr) {
                auto reconnecting = !is_accepted;

                if (reconnecting)
                    schedule_reconnection(psock->saddr());
            }
        }

        destroy_channel(sid);
    }

    void destroy_channel (socket_id sid)
    {
        auto res = _channels.has_channel(sid);
        auto success = (res.first && _channels.close_channel(res.second));

        if (success)
            _on_channel_destroyed(res.second, _index);
        else
            close_socket(sid);
    }

    void process_alive_info (socket_id sid, alive_info<node_id> const & ainfo)
    {
        if (_on_alive_received) {
            auto id_ptr = _channels.locate_reader(sid);

            if (id_ptr != nullptr)
                _on_alive_received(*id_ptr, _index, ainfo);
        }
    }

    void process_unreachable_info (socket_id sid, unreachable_info<node_id> const & uinfo)
    {
        if (_on_unreachable_received) {
            auto id_ptr = _channels.locate_reader(sid);

            if (id_ptr != nullptr)
                _on_unreachable_received(*id_ptr, _index, uinfo);
        }
    }

    void process_route_info (socket_id sid, bool is_response, route_info<node_id> const & rinfo)
    {
        if (_on_route_received) {
            auto id_ptr = _channels.locate_reader(sid);

            if (id_ptr != nullptr)
                _on_route_received(*id_ptr, _index, is_response, rinfo);
        }
    }

    void process_message_received (socket_id sid, int priority, std::vector<char> && bytes)
    {
        if (_on_domestic_data_received) {
            auto id_ptr = _channels.locate_reader(sid);

            if (id_ptr != nullptr)
                _on_domestic_data_received(*id_ptr, priority, std::move(bytes));
        }
    }

    void process_message_received (socket_id sid, int priority, node_id sender_id
        , node_id receiver_id, std::vector<char> && bytes)
    {
        if (_on_global_data_received) {
            auto id_ptr = _channels.locate_reader(sid);

            if (id_ptr != nullptr)
                _on_global_data_received(*id_ptr, priority, sender_id, receiver_id, std::move(bytes));
        }
    }

    void forward_global_packet (int priority, node_id sender_id, node_id receiver_id
        , std::vector<char> && packet)
    {
        if (_on_forward_global_packet)
            _on_forward_global_packet(priority, sender_id, receiver_id, std::move(packet));
    }

    HandshakeController<node> & handshake_processor ()
    {
        return _handshake_controller;
    }

    HeartbeatController<node> & heartbeat_processor ()
    {
        return _heartbeat_controller;
    }

public: // Below methods are for internal use only
    void enqueue_private (socket_id sid, int priority, char const * data, std::size_t len)
    {
        _writer_pool.enqueue(sid, priority, data, len);
    }

    void enqueue_private (socket_id sid, int priority, std::vector<char> data)
    {
        _writer_pool.enqueue(sid, priority, std::move(data));
    }

public: // node_interface
    template <class Node>
    class node_interface_impl: public node_interface<node_id>, protected Node
    {
        using node_id = typename Node::node_id;

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

        void enqueue (node_id id, int priority, bool force_checksum, char const * data
            , std::size_t len) override
        {
            Node::enqueue(id, priority, force_checksum, data, len);
        }

        void enqueue (node_id id, int priority, bool force_checksum, std::vector<char> data) override
        {
            Node::enqueue(id, priority, force_checksum, std::move(data));
        }

        bool has_writer (node_id id) const override
        {
            return Node::has_writer(id);
        }

        void set_frame_size (node_id id, std::uint16_t frame_size) override
        {
            Node::set_frame_size(id, frame_size);
        }

        unsigned int step () override
        {
            return Node::step();
        }

        void clear_channels () override
        {
            Node::clear_channels();
        }

        bool enqueue_packet (node_id id, int priority, std::vector<char> data) override
        {
            return Node::enqueue_packet(id, priority, std::move(data));
        }

        bool enqueue_packet (node_id id, int priority, char const * data, std::size_t len) override
        {
            return Node::enqueue_packet(id, priority, data, len);
        }

        void enqueue_broadcast_packet (int priority, char const * data, std::size_t len) override
        {
            Node::enqueue_broadcast_packet(priority, data, len);
        }

        void enqueue_forward_packet (node_id sender_id, int priority, char const * data
            , std::size_t len) override
        {
            Node::enqueue_forward_packet(sender_id, priority, data, len);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Callback assign methods
        ////////////////////////////////////////////////////////////////////////////////////////////
        void on_error (callback_t<void (std::string const &)> cb) override
        {
            Node::on_error(std::move(cb));
        }

        void on_channel_established (callback_t<void (node_id, node_index_t, bool /*is_gateway*/)> cb) override
        {
            Node::on_channel_established(std::move(cb));
        }

        void on_channel_destroyed (callback_t<void (node_id, node_index_t)> cb) override
        {
            Node::on_channel_destroyed(std::move(cb));
        }

        void on_reconnection_started (callback_t<void (node_index_t, socket4_addr, inet4_addr)> cb) override
        {
            Node::on_reconnection_started(std::move(cb));
        }

        void on_reconnection_stopped (callback_t<void (node_index_t, socket4_addr, inet4_addr)> cb) override
        {
            Node::on_reconnection_stopped(std::move(cb));
        }

        void on_duplicate_id (callback_t<void (node_id, node_index_t, socket4_addr)> cb) override
        {
            Node::on_duplicate_id(std::move(cb));
        }

        void on_bytes_written (callback_t<void (node_id, node_index_t, std::uint64_t)> cb) override
        {
            Node::on_bytes_written(std::move(cb));
        }

        void on_alive_received (callback_t<void (node_id, node_index_t
            , alive_info<node_id> const &)> cb) override
        {
            Node::on_alive_received(std::move(cb));
        }

        void on_unreachable_received (callback_t<void (node_id, node_index_t
            , unreachable_info<node_id> const &)> cb) override
        {
            Node::on_unreachable_received(std::move(cb));
        }

        void on_route_received (callback_t<void (node_id, node_index_t, bool
            , route_info<node_id> const &)> cb) override
        {
            Node::on_route_received(std::move(cb));
        }

        void on_domestic_data_received (callback_t<void (node_id, int
            , std::vector<char>)> cb) override
        {
            Node::on_domestic_data_received(std::move(cb));
        }

        void on_global_data_received (callback_t<void (node_id /*last transmitter node*/
            , int /*priority*/, node_id /*sender ID*/, node_id /*receiver ID*/
            , std::vector<char>)> cb) override
        {
            Node::on_global_data_received(std::move(cb));
        }

        void on_forward_global_packet (callback_t<void (int /*priority*/, node_id /*sender ID*/
            , node_id /*receiver ID*/, std::vector<char>)> cb) override
        {
            Node::on_forward_global_packet(std::move(cb));
        }
    };

    template <typename ...Args>
    static std::unique_ptr<node_interface<node_id>>
    make_interface (Args &&... args)
    {
        return std::unique_ptr<node_interface<node_id>>(
            new node_interface_impl<node_class>(std::forward<Args>(args)...));
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
