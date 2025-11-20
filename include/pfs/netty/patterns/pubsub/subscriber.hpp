////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../callback.hpp"
#include "../../conn_status.hpp"
#include "../../connecting_pool.hpp"
#include "../../connection_failure_reason.hpp"
#include "../../interruptable.hpp"
#include "../../reader_pool.hpp"
#include "../../socket_pool.hpp"
#include "../../socket4_addr.hpp"
#include "../../trace.hpp"
#include "../../traits/archive_traits.hpp"
#include "tag.hpp"
#include <pfs/countdown_timer.hpp>
#include <pfs/i18n.hpp>
#include <pfs/log.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace pubsub {

//
//     +--------- connect ---------+
//     |                           |
//     |                           v
// subscriber                  publisher 1..N
//     ^                           |
//     |                           |
//     +---------- data -----------+
//

template <typename Socket
    , typename ConnectingPoller
    , typename ReaderPoller
    , typename InputController>
class subscriber: public interruptable
{
    using socket_type = Socket;
    using connecting_pool_type = netty::connecting_pool<socket_type, ConnectingPoller>;
    using input_controller_type = InputController;
    using archive_type = typename input_controller_type::archive_type;
    using archive_traits_type = archive_traits<archive_type>;
    using socket_pool_type = netty::socket_pool<socket_type>;
    using reader_pool_type = netty::reader_pool<socket_type, ReaderPoller, archive_type>;

public:
    using socket_id = typename socket_type::socket_id;

private:
    connecting_pool_type  _connecting_pool;
    reader_pool_type      _reader_pool;
    socket_pool_type      _socket_pool;
    input_controller_type _input_controller;

private:
    callback_t<void (std::string const &)> _on_error
        = [] (std::string const & errstr) { LOGE(PUBSUB_TAG, "{}", errstr); };

    callback_t<void (socket4_addr)> _on_connected;
    callback_t<void (socket4_addr)> _on_disconnected;

public:
    subscriber ()
        : interruptable()
    {
        _connecting_pool.on_failure = [this] (netty::error const & err)
        {
            _on_error(tr::f_("connecting pool failure: {}", err.what()));
        };

        _connecting_pool.on_connected = [this] (socket_type && sock)
        {
            NETTY__TRACE(PUBSUB_TAG, "subscriber socket connected: #{}: {}"
                , sock.id()
                , to_string(sock.saddr()));

            PFS__THROW_UNEXPECTED(_socket_pool.count() == 0, "Fix pubsub::subscriber algorithm");

            auto saddr = sock.saddr();

            _reader_pool.add(sock.id());
            _socket_pool.add_connected(std::move(sock));

            if (_on_connected)
                _on_connected(saddr);
        };

        _connecting_pool.on_connection_refused = [this] (netty::socket4_addr saddr
            , netty::connection_failure_reason reason)
        {
            _on_error(tr::f_("connection refused for socket: {}: reason: {}"
                , to_string(saddr), to_string(reason)));
        };

        _reader_pool.on_failure = [this] (socket_id sid, netty::error const & err)
        {
            _on_error(tr::f_("read from socket failure: #{}: {}", sid, err.what()));
            close_socket(sid);
        };

        _reader_pool.on_disconnected = [this] (socket_id sid)
        {
            auto psock = _socket_pool.locate(sid);

            PFS__THROW_UNEXPECTED(psock != nullptr, "Fix pubsub::subscriber algorithm");

            NETTY__TRACE(PUBSUB_TAG, "reader socket disconnected: {} (#{})"
                , to_string(psock->saddr()), sid);

            auto saddr = psock->saddr();

            close_socket(sid);

            if (_on_disconnected)
                _on_disconnected(saddr);
        };

        _reader_pool.on_data_ready = [this] (socket_id /*sid*/, archive_type data)
        {
            _input_controller.process_input(std::move(data));
        };

        _reader_pool.locate_socket = [this] (socket_id sid)
        {
            return _socket_pool.locate(sid);
        };

        NETTY__TRACE(PUBSUB_TAG, "subscriber constructed");
    }

    subscriber (subscriber const &) = delete;
    subscriber (subscriber &&) = delete;
    subscriber & operator = (subscriber const &) = delete;
    subscriber & operator = (subscriber &&) = delete;

    ~subscriber ()
    {
        NETTY__TRACE(PUBSUB_TAG, "subscriber destroyed");
    }

public: // Set callbacks
    /**
     * Sets error callback.
     *
     * @details Callback @a f signature must match:
     *          void (std::string const &)
     */
    template <typename F>
    subscriber & on_error (F && f)
    {
        _on_error = std::forward<F>(f);
        return *this;
    }

    template <typename F>
    subscriber & on_connected (F && f)
    {
        _on_connected = std::forward<F>(f);
        return *this;
    }

    template <typename F>
    subscriber & on_disconnected (F && f)
    {
        _on_disconnected = std::forward<F>(f);
        return *this;
    }

    template <typename F>
    subscriber & on_data_ready (F && f)
    {
        _input_controller.on_data_ready = std::forward<F>(f);
        return *this;
    }

public:
    /**
     * Connects to publisher.
     */
    bool connect (socket4_addr remote_saddr)
    {
        auto rs = _connecting_pool.connect(remote_saddr);

        if (rs == netty::conn_status::failure)
            return false;

        return true;
    }

    /**
     * Connects to publisher.
     */
    bool connect (netty::socket4_addr remote_saddr, netty::inet4_addr local_addr)
    {
        auto rs = _connecting_pool.connect(remote_saddr, local_addr);

        if (rs == netty::conn_status::failure)
            return false;

        return true;
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step ()
    {
        unsigned int result = 0;

        result += _connecting_pool.step();
        result += _reader_pool.step();

        // Remove trash
        _connecting_pool.apply_remove();
        _reader_pool.apply_remove();
        _socket_pool.apply_remove(); // Must be last in the removing sequence

        return result;
    }

    void run (std::chrono::milliseconds loop_interval = std::chrono::milliseconds{10})
    {
        clear_interrupted();

        while (!interrupted()) {
            pfs::countdown_timer<std::milli> countdown_timer {loop_interval};
            auto n = step();

            if (n == 0)
                std::this_thread::sleep_for(countdown_timer.remain());
        }
    }

private:
    void close_socket (socket_id sid)
    {
        if (_socket_pool.locate(sid) != nullptr) {
            _reader_pool.remove_later(sid);
            _socket_pool.remove_later(sid);
        }
    }
};

}} // namespace patterns::pubsub

NETTY__NAMESPACE_END
