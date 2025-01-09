////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
// FIXME DEPRECATED
#pragma once
#include "chrono.hpp"
#include "error.hpp"
#include "exports.hpp"
#include "listener_poller.hpp"
#include "reader_poller.hpp"
#include "writer_poller.hpp"
#include <pfs/stopwatch.hpp>
#include <functional>
#include <set>
#include <utility>
#include <vector>

namespace netty {

/**
 * Connection oriented server poller
 */
template <typename Backend>
class server_poller
{
public:
    using backend_type = Backend;
    using listener_id = typename Backend::listener_id;
    using socket_id = typename Backend::socket_id;

private:
    listener_poller<Backend> _listener_poller;
    reader_poller<Backend> _reader_poller;
    writer_poller<Backend> _writer_poller;
    std::vector<socket_id> _addable_listeners;
    std::vector<socket_id> _addable_readers;
    std::vector<socket_id> _removable_listeners;
    std::vector<socket_id> _removable_readers;
    std::vector<socket_id> _removable_writers;
    std::set<socket_id>    _removable;

    // Reader and writer pollers backends are shared or not
    bool _is_pollers_shared {false};

public: // callbacks
    mutable std::function<void(socket_id, error const &)> on_listener_failure
        = [] (socket_id, error const &) {};
    mutable std::function<void(socket_id, error const &)> on_failure
        = [] (socket_id, error const &) {};
    mutable std::function<void(socket_id)> ready_read = [] (socket_id) {};
    mutable std::function<void(socket_id)> accepted = [] (socket_id) {};
    mutable std::function<void(socket_id)> disconnected = [] (socket_id) {};
    mutable std::function<void(socket_id)> can_write = [] (socket_id) {};
    mutable std::function<void(socket_id)> listener_removed = [] (socket_id) {};
    mutable std::function<void(socket_id)> removed = [] (socket_id) {};

private:
    std::function<socket_id(socket_id, bool &)> accept;

private:
    void init_callbacks (std::function<socket_id(socket_id, bool &)> && accept_proc)
    {
        _listener_poller.on_failure = [this] (listener_id sock, error const & err) {
            // Socket must be removed from monitoring later
            _removable_listeners.push_back(sock);
            on_listener_failure(sock, err);
        };

        _reader_poller.on_failure = [this] (socket_id sock, error const & err) {
            // Socket must be removed from monitoring later
            _removable_readers.push_back(sock);
            _removable.insert(sock);
            on_failure(sock, err);
        };

        accept = accept_proc;

        _listener_poller.accept = [this] (listener_id listener_sock) {
            bool ok = true;
            auto sock = this->accept(listener_sock, ok);

            if (ok) {
                accepted(sock);
                _addable_readers.push_back(sock);
            }
        };

        _reader_poller.ready_read = [this] (socket_id sock) {
            ready_read(sock);
        };

        _reader_poller.disconnected = [this] (socket_id sock) {
            _removable_readers.push_back(sock);
            _removable.insert(sock);
            disconnected(sock);
        };

        _writer_poller.on_failure = [this] (socket_id sock, error const & err) {
            _removable_writers.push_back(sock);
            _removable.insert(sock);
            on_failure(sock, err);
        };

        _writer_poller.can_write = [this] (socket_id sock) {
            // If writer poller is shared, so no need to remove socket from it
            if (!_is_pollers_shared)
                _removable_writers.push_back(sock);

            can_write(sock);
        };
    }

    server_poller (std::shared_ptr<Backend> shared_poller_backend
        , std::function<socket_id(socket_id, bool &)> && accept_proc)
        : _listener_poller(shared_poller_backend)
        , _reader_poller(shared_poller_backend)
        , _writer_poller(shared_poller_backend)
        , _is_pollers_shared(true)
    {
        init_callbacks(std::move(accept_proc));
    }

public:
    NETTY__EXPORT server_poller (std::function<socket_id(socket_id, bool &)> && accept_proc);
    ~server_poller () = default;

    server_poller (server_poller const &) = delete;
    server_poller & operator = (server_poller const &) = delete;
    server_poller (server_poller &&) = default;
    server_poller & operator = (server_poller &&) = delete;

    /**
     * Add listener socket.
     */
    template <typename Listener>
    void add_listener (Listener const & listener, error * perr = nullptr)
    {
        _addable_listeners.push_back(listener.id());
    }

    /**
     * Remove listener socket.
     */
    template <typename Listener>
    void remove_listener (Listener const & listener, error * perr = nullptr)
    {
        _removable_listeners.push_back(listener.id());
    }

    /**
     * Remove reader socket.
     */
    template <typename Socket>
    void remove (Socket const & sock, error * /*perr*/ = nullptr)
    {
        _removable_readers.push_back(sock.id());
        _removable_writers.push_back(sock.id());
        _removable.insert(sock.id());
    }

    /**
     * Check if listener and regular pollers are empty.
     */
    bool empty () const noexcept
    {
        return _listener_poller.empty()
            && _reader_poller.empty()
            && _writer_poller.empty();
    }

    template <typename Socket>
    void wait_for_write (Socket const & sock, error * perr = nullptr)
    {
        _writer_poller.wait_for_write(sock.id(), perr);
    }

    /**
     * @return Total number of positive events: number of pending connections
     *         plus number of read events.
     */
    int poll (std::chrono::milliseconds timeout = std::chrono::milliseconds{0}
        , error * perr = nullptr)
    {
        static std::chrono::milliseconds __zero_millis{0};

        int n1 = 0;
        int n2 = 0;

        pfs::stopwatch<std::milli> stopwatch;

        // The order of call polls is matter

        if (!_writer_poller.empty()) {
            stopwatch.start();
            n1 = _writer_poller.poll(timeout, perr);
            stopwatch.stop();
            timeout -= std::chrono::milliseconds{stopwatch.count()};

            if (timeout < __zero_millis)
                timeout = __zero_millis;
        }

        if (!_reader_poller.empty()) {
            stopwatch.start();
            n1 = _reader_poller.poll(timeout, perr);
            stopwatch.stop();
            timeout -= std::chrono::milliseconds{stopwatch.count()};

            if (timeout < __zero_millis)
                timeout = __zero_millis;
        }

        if (!_listener_poller.empty())
            n2 = _listener_poller.poll(timeout, perr);

        if (!_addable_listeners.empty()) {
            for (auto sock: _addable_listeners)
                _listener_poller.add(sock, perr);
            _addable_listeners.clear();
        }

        if (!_addable_readers.empty()) {
            for (auto sock: _addable_readers)
                _reader_poller.add(sock, perr);
            _addable_readers.clear();
        }

        if (!_removable_listeners.empty()) {
            for (auto const & sock: _removable_listeners) {
                _listener_poller.remove(sock);
                listener_removed(sock);
            }
            _removable_listeners.clear();
        }

        if (!_removable_readers.empty()) {
            for (auto const & sock: _removable_readers)
                _reader_poller.remove(sock);
            _removable_readers.clear();
        }

        if (!_removable_writers.empty()) {
            for (auto sock: _removable_writers)
                _writer_poller.remove(sock);
            _removable_writers.clear();
        }

        if (!_removable.empty()) {
            for (auto sock: _removable)
                removed(sock);
            _removable.clear();
        }

        if (n1 < 0 && n2 < 0)
            return n1;

        return (n1 > 0 ? n1 : 0)
            + (n2 > 0 ? n2 : 0);
    }
};

} // namespace netty
