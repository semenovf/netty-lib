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
#include <functional>
#include <utility>

namespace netty {

/**
 * Connection oriented server poller
 */
template <typename Backend>
class server_poller
{
public:
    using native_socket_type = typename Backend::native_socket_type;

    struct callbacks
    {
        std::function<void(native_socket_type, std::string const &)> on_error;
        std::function<native_socket_type(native_socket_type, bool &)> accept;
        std::function<void(native_socket_type)> disconnected;
        std::function<void(native_socket_type)> ready_read;
    };

private:
    listener_poller<Backend> _listener_poller;
    reader_poller<Backend>   _reader_poller;

private: // callbacks
    std::function<native_socket_type(native_socket_type, bool &)> accept;

public:
    server_poller (callbacks && cbs)
    {
        if (cbs.on_error) {
            _listener_poller.on_error = cbs.on_error;
            _reader_poller.on_error = std::move(cbs.on_error);
        }

        accept = std::move(cbs.accept);

        _listener_poller.accept = [this] (native_socket_type listener_sock) {
            bool ok = true;
            auto sock = this->accept(listener_sock, ok);

            if (ok)
                _reader_poller.add(sock);
        };

        _reader_poller.disconnected = std::move(cbs.disconnected);
        _reader_poller.ready_read = std::move(cbs.ready_read);
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
    int poll (std::chrono::milliseconds millis = std::chrono::milliseconds{0}
        , error * perr = nullptr)
    {
        int n1 = 0;
        int n2 = 0;

        auto empty_reader_poller = _reader_poller.empty();

        if (!_listener_poller.empty()) {
            if (empty_reader_poller)
                n1 = _listener_poller.poll(millis, perr);
            else
                n1 = _listener_poller.poll(std::chrono::milliseconds{0}, perr);
        }

        if (!empty_reader_poller) {
            n2 = _reader_poller.poll(millis, perr);
        }

        if (n1 < 0 && n2 < 0)
            return n1;

        return (n1 > 0 ? n1 : 0)
            + (n2 > 0 ? n2 : 0);
    }
};

} // namespace netty
