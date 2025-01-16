////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/namespace.hpp>
#include <pfs/stopwatch.hpp>
#include <pfs/netty/conn_status.hpp>
#include <pfs/netty/connection_refused_reason.hpp>
#include <pfs/netty/error.hpp>
#include <pfs/netty/socket4_addr.hpp>
#include <pfs/netty/connecting_pool.hpp>
#include <pfs/netty/listener_pool.hpp>
#include <pfs/netty/reader_pool.hpp>
#include <pfs/netty/socket_pool.hpp>
#include <pfs/netty/writer_pool.hpp>

#include <pfs/log.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename UniversalId
    , typename Listener
    , typename Socket
    , typename ConnectingPoller
    , typename ListenerPoller
    , typename ReaderPoller
    , typename WriterPoller>
class node
{
    using universal_id_type = UniversalId;
    using listener_type = Listener;
    using socket_type = Socket;
    using socket_pool_type = netty::socket_pool<socket_type>;
    using connecting_pool_type = netty::connecting_pool<ConnectingPoller, socket_type>;
    using listener_pool_type = netty::listener_pool<ListenerPoller, listener_type, socket_type>;
    using reader_pool_type = netty::reader_pool<ReaderPoller, socket_type>;
    using writer_pool_type = netty::writer_pool<WriterPoller, socket_type>;

    using socket_id = typename socket_type::socket_id;
    using listener_id = typename listener_type::listener_id;

private:
    universal_id_type    _node_id;
    listener_pool_type   _listener_pool;
    connecting_pool_type _connecting_pool;
    reader_pool_type     _reader_pool;
    writer_pool_type     _writer_pool;
    socket_pool_type     _socket_pool;

    std::chrono::seconds _reconnect_timeout {5};

private:
    void schedule_reconnection (socket_id id)
    {
        bool is_accepted = false;
        auto psock = _socket_pool.locate(id, & is_accepted);
        auto reconnecting = !is_accepted;

        if (reconnecting)
            _connecting_pool.connect_timeout(_reconnect_timeout, psock->saddr());

        _socket_pool.close(id);
    }

public:
    node (universal_id_type node_id, std::chrono::seconds reconnect_timeout = std::chrono::seconds{5})
        : _node_id(node_id)
        , _reconnect_timeout(reconnect_timeout)
    {
        _listener_pool.on_failure([] (netty::error const & err) {
            LOGE("", "listener pool failure: {}", err.what());
        }).on_accepted([this] (socket_type && sock) {
            LOGD("", "socket accepted: {}#{}", to_string(sock.saddr()), sock.id());
            _reader_pool.add(sock.id());
            _socket_pool.add_accepted(std::move(sock));
        });

        _connecting_pool.on_failure([] (netty::error const & err) {
            LOGE("", "connecting pool failure: {}", err.what());
        }).on_connected([this] (socket_type && sock) {
            LOGD("", "socket connected: {}#{}", to_string(sock.saddr()), sock.id());
            _reader_pool.add(sock.id());
            _socket_pool.add_connected(std::move(sock));

            // FIXME
        //     netty::p2p::hello_packet packet;
        //     packet.uuid = host_id;
        //     serializer_t::ostream_type out;
        //     serializer_t::pack(out, packet);
        //     writer_pool.enqueue(sock_id, out.data(), out.size());
        }).on_connection_refused ([this] (socket_id sid, netty::socket4_addr saddr
                , netty::connection_refused_reason reason) {
            LOGE("", "connection refused for socket ({}): {}: reason: {}"
                ", reconnecting after {}"
                , sid, to_string(saddr), to_string(reason), _reconnect_timeout);
            _connecting_pool.connect_timeout(_reconnect_timeout, saddr);
        });

        _reader_pool.on_failure([this] (socket_id sid, netty::error const & err) {
            LOGE("", "read from socket failure: {}: {}", sid, err.what());
            _socket_pool.close(sid);
        }).on_disconnected([this] (socket_id sid) {
            LOGD("", "socket disconnected: #{}", sid);
            schedule_reconnection(sid);
        }).on_ready([] (socket_id sid, std::vector<char> && data) {
            // FIXME
        //     LOGD(TAG, "{:04}: Input data ready: id={}: {} bytes", self_port, sid, data.size());
        }).on_locate_socket([this] (socket_id sid) {
            return _socket_pool.locate(sid);
        });

        _writer_pool.on_failure([this] (socket_id sid, netty::error const & err) {
            LOGE("", "write to socket failure: socket={}: {}", sid, err.what());
            schedule_reconnection(sid);
        }).on_bytes_written([] (socket_id sid, std::uint64_t n) {
            // FIXME
        //     LOGD(TAG, "{:04}: bytes written: id={}: {}", self_port, id, n);
        }).on_locate_socket([this] (socket_id sid) {
            return _socket_pool.locate(sid);
        });
    }

    ~node () {}

public:
    void add_listener (netty::socket4_addr const & listener_addr, error * perr = nullptr)
    {
        _listener_pool.add(listener_addr, perr);
    }

    bool connect_host (netty::socket4_addr addr)
    {
        netty::error err;
        auto rs = _connecting_pool.connect(addr);
        return rs == netty::conn_status::failure;
    }

    void listen (int backlog = 50)
    {
        _listener_pool.listen(backlog);
    }

    void step (std::chrono::milliseconds millis)
    {
        pfs::stopwatch<std::milli> stopwatch;

        _listener_pool.step();
        _connecting_pool.step();
        _writer_pool.step();

        millis -= std::chrono::milliseconds{stopwatch.current_count()};

        if (millis <= std::chrono::milliseconds{0})
            millis = std::chrono::milliseconds{0};

        _reader_pool.step(millis);
        _socket_pool.step();
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
