////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
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

#include <pfs/log.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace meshnet {

template <typename NodeIdintifier
    , typename Listener
    , typename Socket
    , typename ConnectingPoller
    , typename ListenerPoller
    , typename ReaderPoller
    , typename WriterPoller
    , typename WriterQueue
    , typename SerializerTraits
    , template <typename> class HeartBeatGenerator
    , template <typename> class InputProcessor
    , template <typename> class Callbacks>
class node
{
    using listener_type = Listener;
    using socket_type = Socket;
    using socket_pool_type = netty::socket_pool<socket_type>;
    using connecting_pool_type = netty::connecting_pool<socket_type, ConnectingPoller>;
    using listener_pool_type = netty::listener_pool<listener_type, socket_type, ListenerPoller>;
    using reader_pool_type = netty::reader_pool<socket_type, ReaderPoller>;
    using writer_pool_type = netty::writer_pool<socket_type, WriterPoller, WriterQueue>;
    using listener_id = typename listener_type::listener_id;

public:
    using node_id = NodeIdintifier;
    using socket_id = typename socket_type::socket_id;
    using serializer_traits = SerializerTraits;
    using callback_traits = Callbacks<node>;

private:
    NodeIdintifier       _id;
    listener_pool_type   _listener_pool;
    connecting_pool_type _connecting_pool;
    reader_pool_type     _reader_pool;
    writer_pool_type     _writer_pool;
    socket_pool_type     _socket_pool;

    std::chrono::seconds _reconnection_timeout {5};
    HeartBeatGenerator<node> _heartbeat_generator;
    InputProcessor<node> _input_processor;
    callback_traits _cb;

public:
    node (NodeIdintifier id, callback_traits && cb)
        : node(id, std::chrono::seconds{5}, std::move(cb))
    {}

    node (NodeIdintifier id, std::chrono::seconds reconnection_timeout, callback_traits && cb)
        : _id(id)
        , _reconnection_timeout(reconnection_timeout)
        , _heartbeat_generator(*this)
        , _cb(std::move(cb))
    {
        if (_reconnection_timeout < std::chrono::seconds{0})
            _reconnection_timeout = std::chrono::seconds{0};

        if (_reconnection_timeout > std::chrono::seconds{3600 * 24})
            _reconnection_timeout = std::chrono::seconds{3600 * 24};

        _listener_pool.on_failure([this] (netty::error const & err) {
            if (_cb.on_error)
                _cb.on_error(tr::f_("listener pool failure: {}", err.what()));
        }).on_accepted([this] (socket_type && sock) {
            LOGD("", "socket accepted: {}#{}", to_string(sock.saddr()), sock.id());
            _heartbeat_generator.add(sock.id());
            _input_processor.add(sock.id());
            _reader_pool.add(sock.id());
            _socket_pool.add_accepted(std::move(sock));
        });

        _connecting_pool.on_failure([this] (netty::error const & err) {
            if (_cb.on_error)
                _cb.on_error(tr::f_("connecting pool failure: {}", err.what()));
        }).on_connected([this] (socket_type && sock) {
            LOGD("", "socket connected: {}#{}", to_string(sock.saddr()), sock.id());
            _heartbeat_generator.add(sock.id());
            _input_processor.add(sock.id());
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

            if (_reconnection_timeout > std::chrono::seconds{0})
                _connecting_pool.connect_timeout(_reconnection_timeout, saddr);
        });

        _reader_pool.on_failure([this] (socket_id sid, netty::error const & err) {
            if (_cb.on_error)
                _cb.on_error(tr::f_("read from socket failure: {}: {}", sid, err.what()));

            _heartbeat_generator.remove(sid);
            _input_processor.remove(sid);
            _writer_pool.remove_later(sid);
            _socket_pool.remove_later(sid);
        }).on_disconnected([this] (socket_id sid) {
            LOGD("", "socket disconnected: #{}", sid);

            schedule_reconnection(sid);

            _heartbeat_generator.remove(sid);
            _input_processor.remove(sid);
            _writer_pool.remove_later(sid);
            _socket_pool.remove_later(sid);
        }).on_data_ready([this] (socket_id sid, std::vector<char> && data) {
            _input_processor.process_input(sid, std::move(data));
        }).on_locate_socket([this] (socket_id sid) {
            return _socket_pool.locate(sid);
        });

        _writer_pool.on_failure([this] (socket_id sid, netty::error const & err) {
            if (_cb.on_error)
                _cb.on_error(tr::f_("write to socket failure: socket={}: {}", sid, err.what()));

            schedule_reconnection(sid);

            _heartbeat_generator.remove(sid);
            _input_processor.remove(sid);
            _reader_pool.remove_later(sid);
            _socket_pool.remove_later(sid);
        }).on_bytes_written([] (socket_id sid, std::uint64_t n) {
            // FIXME Use this callback to collect statistics
            LOGD("", "bytes written: id={}: {}", sid, n);
        }).on_locate_socket([this] (socket_id sid) {
            return _socket_pool.locate(sid);
        });
    }

    ~node () {}

public:
    template <typename ...Args>
    void configure_heartbeat_processor (Args &&... args)
    {
        _heartbeat_generator.configure(std::forward<Args>(args)...);
    }

    template <typename ...Args>
    void configure_input_processor (Args &&... args)
    {
        _input_processor.configure(std::forward<Args>(args)...);
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

    void send (socket_id id, int priority, char const * data, std::size_t len)
    {
        _writer_pool.enqueue(id, priority, data, len);
    }

    void send (socket_id id, int priority, std::vector<char> && data)
    {
        _writer_pool.enqueue(id, priority, std::move(data));
    }

    void listen (int backlog = 50)
    {
        _listener_pool.listen(backlog);
    }

    void step (std::chrono::milliseconds millis)
    {
        pfs::countdown_timer<std::milli> countdown_timer {millis};

        _listener_pool.step();
        _connecting_pool.step();
        _writer_pool.step();

        millis = countdown_timer.remain();

        _reader_pool.step(millis);
        _heartbeat_generator.step();

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
    void schedule_reconnection (socket_id sid)
    {
        if (_reconnection_timeout > std::chrono::seconds{0}) {
            bool is_accepted = false;
            auto psock = _socket_pool.locate(sid, & is_accepted);
            auto reconnecting = !is_accepted;

            PFS__TERMINATE(psock != nullptr, "Fix meshnet::node algorithm");

            if (reconnecting)
                _connecting_pool.connect_timeout(_reconnection_timeout, psock->saddr());
        }
    }
};

}} // namespace patterns::meshnet

NETTY__NAMESPACE_END
