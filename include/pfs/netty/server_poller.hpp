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
#include "pfs/stopwatch.hpp"
#include <functional>
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

private:
    listener_poller<Backend> _listener_poller;
    reader_poller<Backend>   _reader_poller;
    std::vector<native_socket_type> _removable_listeners;
    std::vector<native_socket_type> _removable_readers;

public: // callbacks
    mutable std::function<void(native_socket_type, std::string const &)> on_listener_failure
        = [] (native_socket_type, std::string const &) {};
    mutable std::function<void(native_socket_type, std::string const &)> on_reader_failure
        = [] (native_socket_type, std::string const &) {};
    mutable std::function<void(native_socket_type)> ready_read = [] (native_socket_type) {};
    mutable std::function<void(native_socket_type)> disconnected = [] (native_socket_type) {};

private:
    std::function<native_socket_type(native_socket_type, bool &)> accept;

public:
    server_poller (std::function<native_socket_type(native_socket_type, bool &)> && accept_proc)
    {
        _listener_poller.on_failure = [this] (native_socket_type sock, std::string const & errstr) {
            // Socket must be removed from monitoring later
            _removable_listeners.push_back(sock);
            on_listener_failure(sock, errstr);
        };

        _reader_poller.on_failure = [this] (native_socket_type sock, std::string const & errstr) {
            // Socket must be removed from monitoring later
            _removable_readers.push_back(sock);
            on_reader_failure(sock, errstr);
        };

        accept = accept_proc;

        _listener_poller.accept = [this] (native_socket_type listener_sock) {
            bool ok = true;
            auto sock = this->accept(listener_sock, ok);

            if (ok)
                _reader_poller.add(sock);
        };

        _reader_poller.ready_read = [this] (native_socket_type sock) {
            ready_read(sock);
        };

        _reader_poller.disconnected = [this] (native_socket_type sock) {
            // Socket must be removed from monitoring later
            _removable_readers.push_back(sock);
            disconnected(sock);
        };
    }

    ~server_poller () = default;

    server_poller (server_poller const &) = delete;
    server_poller & operator = (server_poller const &) = delete;
    server_poller (server_poller &&) = delete;
    server_poller & operator = (server_poller &&) = delete;

    /**
     * Add listener socket.
     */
    template <typename Listener>
    void add_listener (Listener const & listener, error * perr = nullptr)
    {
        _listener_poller.add(listener.native(), perr);
    }

    /**
     * Remove listener socket.
     */
    template <typename Listener>
    void remove_listener (Listener const & listener, error * perr = nullptr)
    {
        _listener_poller.remove(listener.native(), perr);
    }

    /**
     * Remove reader socket.
     */
    template <typename Socket>
    void remove (Socket const & sock, error * perr = nullptr)
    {
        _reader_poller.remove(sock.native(), perr);
    }

    /**
     * Check if listener and regular pollers are empty.
     */
    bool empty () const noexcept
    {
        return _listener_poller.empty() && _reader_poller.empty();
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

        if (!_removable_listeners.empty()) {
            for (auto const & sock: _removable_listeners)
                _listener_poller.remove(sock);
            _removable_listeners.clear();
        }

        if (!_removable_readers.empty()) {
            for (auto const & sock: _removable_readers)
                _reader_poller.remove(sock);
            _removable_readers.clear();
        }

        if (n1 < 0 && n2 < 0)
            return n1;

        return (n1 > 0 ? n1 : 0)
            + (n2 > 0 ? n2 : 0);
    }
};

} // namespace netty
