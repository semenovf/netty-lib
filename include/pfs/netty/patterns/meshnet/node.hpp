////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "functional_callbacks.hpp"
#include "without_reconnection.hpp"
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

template <typename Listener
    , typename Socket
    , typename ConnectingPoller
    , typename ListenerPoller
    , typename ReaderPoller
    , typename WriterPoller
    , template <typename> class ReconnectionScheduler = without_reconnection
    , typename Callbacks = functional_callbacks>
class node
{
    friend class ReconnectionScheduler<node>;

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
    listener_pool_type   _listener_pool;
    connecting_pool_type _connecting_pool;
    reader_pool_type     _reader_pool;
    writer_pool_type     _writer_pool;
    socket_pool_type     _socket_pool;

    ReconnectionScheduler<node> _reconnection_scheduler;

public:
    node ()
        : _reconnection_scheduler(*this)
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
                ", reconnecting"
                , sid, to_string(saddr), to_string(reason));
            _reconnection_scheduler(saddr);
        });

        _reader_pool.on_failure([this] (socket_id sid, netty::error const & err) {
            LOGE("", "read from socket failure: {}: {}", sid, err.what());
            _socket_pool.close(sid);
        }).on_disconnected([this] (socket_id sid) {
            LOGD("", "socket disconnected: #{}", sid);
            _reconnection_scheduler(sid);
        }).on_ready([] (socket_id sid, std::vector<char> && data) {
            // FIXME
        //     LOGD(TAG, "{:04}: Input data ready: id={}: {} bytes", self_port, sid, data.size());
        }).on_locate_socket([this] (socket_id sid) {
            return _socket_pool.locate(sid);
        });

        _writer_pool.on_failure([this] (socket_id sid, netty::error const & err) {
            LOGE("", "write to socket failure: socket={}: {}", sid, err.what());
            _reconnection_scheduler(sid);
        }).on_bytes_written([] (socket_id sid, std::uint64_t n) {
            // FIXME
        //     LOGD(TAG, "{:04}: bytes written: id={}: {}", self_port, id, n);
        }).on_locate_socket([this] (socket_id sid) {
            return _socket_pool.locate(sid);
        });
    }

    ~node () {}

public:
    template <typename ...Args>
    void configure_reconnection_scheduler (Args &&... args)
    {
        _reconnection_scheduler.configure(std::forward<Args>(args)...);
    }

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
