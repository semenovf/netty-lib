////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.08.04 Initial version.
//      2025.08.08 `interruptable` inheritance.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include "../../callback.hpp"
#include "../../interruptable.hpp"
#include "../../listener_pool.hpp"
#include "../../socket_pool.hpp"
#include "../../socket4_addr.hpp"
#include "../../trace.hpp"
#include "../../writer_pool.hpp"
#include "tag.hpp"
#include <pfs/countdown_timer.hpp>
#include <pfs/i18n.hpp>
#include <pfs/log.hpp>
#include <chrono>
#include <mutex>
#include <thread>

NETTY__NAMESPACE_BEGIN

namespace patterns {
namespace pubsub {

//
//     +--------- connect ---------+
//     |                           |
//     v                           |
// publisher                  subscriber
//     |                           ^
//     |                           |
//     +---------- data -----------+
//

template <typename Socket
    , typename Listener
    , typename ListenerPoller
    , typename WriterPoller
    , typename WriterQueue
    , typename RecursiveWriterMutex>
class publisher: public interruptable
{
private:
    using socket_type = Socket;
    using listener_type = Listener;
    using socket_pool_type = netty::socket_pool<socket_type>;
    using listener_pool_type = netty::listener_pool<listener_type, socket_type, ListenerPoller>;
    using archive_type = typename WriterQueue::archive_type;
    using writer_pool_type = netty::writer_pool<socket_type, WriterPoller, WriterQueue>;
    using writer_mutex_type = RecursiveWriterMutex;

    using listener_id = typename listener_type::listener_id;
    using socket_id = typename socket_type::socket_id;

private:
    listener_pool_type _listener_pool;
    writer_pool_type   _writer_pool;
    socket_pool_type   _socket_pool;

    // Writer mutex to protect broadcasting
    writer_mutex_type _writer_mtx;

private: // Callbacks
    callback_t<void (std::string const &)> _on_error
        = [] (std::string const & errstr) { LOGE(PUBSUB_TAG, "{}", errstr); };

    callback_t<void (socket4_addr)> _on_accepted;

public:
    publisher (socket4_addr listener_saddr, int backlog = 100)
        : interruptable()
    {
        _listener_pool.on_failure = [this] (netty::error const & err)
        {
            _on_error(tr::f_("listener pool failure: {}", err.what()));
        };

        _listener_pool.on_accepted = [this] (socket_type && sock)
        {
            NETTY__TRACE(PUBSUB_TAG, "subscriber socket accepted: #{}: {}", sock.id(), to_string(sock.saddr()));
            auto saddr = sock.saddr();
            _writer_pool.add(sock.id());
            _socket_pool.add_accepted(std::move(sock));

            if (_on_accepted)
                _on_accepted(saddr);
        };

        _writer_pool.on_failure = [this] (socket_id sid, netty::error const & err)
        {
            _on_error(tr::f_("write to socket failure: #{}: {}", sid, err.what()));
        };

        _writer_pool.on_disconnected = [this] (socket_id sid)
        {
            NETTY__TRACE(PUBSUB_TAG, "subscriber socket disconnected: #{}", sid);
        };

        _writer_pool.locate_socket = [this] (socket_id sid)
        {
            return _socket_pool.locate(sid);
        };

        _listener_pool.add(listener_saddr);
        _listener_pool.listen(backlog);

        NETTY__TRACE(PUBSUB_TAG, "publisher constructed and listen on: {}", to_string(listener_saddr));
    }

    publisher (publisher const &) = delete;
    publisher (publisher &&) = delete;
    publisher & operator = (publisher const &) = delete;
    publisher & operator = (publisher &&) = delete;

    ~publisher ()
    {
        NETTY__TRACE(PUBSUB_TAG, "publisher destroyed");
    }

public: // Set callbacks
    /**
     * Sets error callback.
     *
     * @details Callback @a f signature must match:
     *          void (std::string const &)
     */
    template <typename F>
    publisher & on_error (F && f)
    {
        _on_error = std::forward<F>(f);
        return *this;
    }

    template <typename F>
    publisher & on_accepted (F && f)
    {
        _on_accepted = std::forward<F>(f);
        return *this;
    }

public:
    void broadcast (char const * data, std::size_t size)
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};
        broadcast_unsafe(data, size);
    }

    void broadcast_unsafe (char const * data, std::size_t size)
    {
        _writer_pool.enqueue_broadcast(data, size);
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step ()
    {
        std::unique_lock<writer_mutex_type> locker{_writer_mtx};
        return step_unsafe();
    }

    unsigned int step_unsafe ()
    {
        unsigned int result = 0;

        result += _listener_pool.step();
        result += _writer_pool.step();

        // Remove trash
        _listener_pool.apply_remove();
        _writer_pool.apply_remove();
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
};

}} // namespace patterns::pubsub

NETTY__NAMESPACE_END
