////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "chrono.hpp"
#include "error.hpp"
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
    using native_socket_type = typename Backend::native_socket_type;
    using backend_type = Backend;

private:
    listener_poller<Backend> _listener_poller;
    reader_poller<Backend>   _reader_poller;
    writer_poller<Backend>   _writer_poller;
    std::vector<native_socket_type> _addable_listeners;
    std::vector<native_socket_type> _addable_readers;
    std::vector<native_socket_type> _removable_listeners;
    std::vector<native_socket_type> _removable_readers;
    std::vector<native_socket_type> _removable_writers;
    std::set<native_socket_type> _removable;

    // Reader and writer pollers backends are shared or not
    bool _is_pollers_shared {false};

public: // callbacks
    mutable std::function<void(native_socket_type, error const &)> on_listener_failure
        = [] (native_socket_type, error const &) {};
    mutable std::function<void(native_socket_type, error const &)> on_failure
        = [] (native_socket_type, error const &) {};
    mutable std::function<void(native_socket_type)> ready_read = [] (native_socket_type) {};
    mutable std::function<void(native_socket_type)> accepted = [] (native_socket_type) {};
    mutable std::function<void(native_socket_type)> disconnected = [] (native_socket_type) {};
    mutable std::function<void(native_socket_type)> can_write = [] (native_socket_type) {};
    mutable std::function<void(native_socket_type)> listener_removed = [] (native_socket_type) {};
    mutable std::function<void(native_socket_type)> removed = [] (native_socket_type) {};

private:
    std::function<native_socket_type(native_socket_type, bool &)> accept;

private:
    void init_callbacks (std::function<native_socket_type(native_socket_type, bool &)> && accept_proc)
    {
        _listener_poller.on_failure = [this] (native_socket_type sock, error const & err) {
            // Socket must be removed from monitoring later
            _removable_listeners.push_back(sock);
            on_listener_failure(sock, err);
        };

        _reader_poller.on_failure = [this] (native_socket_type sock, error const & err) {
            // Socket must be removed from monitoring later
            _removable_readers.push_back(sock);
            _removable.insert(sock);
            on_failure(sock, err);
        };

        accept = accept_proc;

        _listener_poller.accept = [this] (native_socket_type listener_sock) {
            bool ok = true;
            auto sock = this->accept(listener_sock, ok);

            if (ok) {
                accepted(sock);
                _addable_readers.push_back(sock);
            }
        };

        _reader_poller.ready_read = [this] (native_socket_type sock) {
            ready_read(sock);
        };

        _reader_poller.disconnected = [this] (native_socket_type sock) {
            _removable_readers.push_back(sock);
            _removable.insert(sock);
            disconnected(sock);
        };

        _writer_poller.on_failure = [this] (native_socket_type sock, error const & err) {
            _removable_writers.push_back(sock);
            _removable.insert(sock);
            on_failure(sock, err);
        };

        _writer_poller.can_write = [this] (native_socket_type sock) {
            // If writer poller is shared, so no need to remove socket from it
            if (!_is_pollers_shared)
                _removable_writers.push_back(sock);

            can_write(sock);
        };
    }

    server_poller (std::shared_ptr<Backend> shared_poller_backend
        , std::function<native_socket_type(native_socket_type, bool &)> && accept_proc)
        : _listener_poller(shared_poller_backend)
        , _reader_poller(shared_poller_backend)
        , _writer_poller(shared_poller_backend)
        , _is_pollers_shared(true)
    {
        init_callbacks(std::move(accept_proc));
    }

public:
    server_poller (std::function<native_socket_type(native_socket_type, bool &)> && accept_proc);
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
        _addable_listeners.push_back(listener.native());
    }

    /**
     * Remove listener socket.
     */
    template <typename Listener>
    void remove_listener (Listener const & listener, error * perr = nullptr)
    {
        _removable_listeners.push_back(listener.native());
    }

    /**
     * Remove reader socket.
     */
    template <typename Socket>
    void remove (Socket const & sock, error * perr = nullptr)
    {
        _removable_readers.push_back(sock.native());
        _removable_writers.push_back(sock.native());
        _removable.insert(sock.native());
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
        _writer_poller.wait_for_write(sock.native(), perr);
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
