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
#include "regular_poller.hpp"
#include <functional>

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
        std::function<void(native_socket_type)> accept;
        //std::function<void(native_socket_type)> disconnected;
        std::function<void(native_socket_type)> ready_read;
        std::function<void(native_socket_type)> can_write;
    };

private:
    listener_poller<Backend> _listener_poller;
    regular_poller<Backend>  _poller;

private: // callbacks
    std::function<void(native_socket_type)> accept;

public:
    server_poller (callbacks && cbs)
        : _listener_poller()
        , _poller()
    {
        if (cbs.on_error) {
            _listener_poller.on_error = cbs.on_error;
            _poller.on_error = std::move(cbs.on_error);
        }

        accept = std::move(cbs.accept);

        _listener_poller.accept = [this] (native_socket_type sock) {
            // sock already removed from _listener_poller
            _poller.add(sock);
            this->accept(sock);
        };

        //_poller.disconnected = std::move(cbs.disconnected);
        _poller.ready_read = std::move(cbs.ready_read);
        _poller.can_write  = std::move(cbs.can_write);
    }

    ~server_poller () = default;

    server_poller (server_poller const &) = delete;
    server_poller & operator = (server_poller const &) = delete;
    server_poller (server_poller &&) = delete;
    server_poller & operator = (server_poller &&) = delete;

    /**
     * Add listener socket.
     */
    void add (native_socket_type listener_sock, error * perr = nullptr)
    {
        _listener_poller.add(listener_sock, perr);
    }

    /**
     * Remove listener socket.
     */
    void remove_listener (native_socket_type listener_sock, error * perr = nullptr)
    {
        _listener_poller.remove(listener_sock, perr);
    }

    /**
     * Remove client socket.
     */
    void remove (native_socket_type sock, error * perr = nullptr)
    {
        _poller.remove(sock, perr);
    }

    /**
     * Check if listener and regular pollers are empty.
     */
    bool empty () const noexcept
    {
        return _listener_poller.empty() && _poller.empty();
    }

    void poll (std::chrono::milliseconds millis, error * perr = nullptr)
    {
        if (!_listener_poller.empty()) {
            if (_poller.empty())
                _listener_poller.poll(millis, perr);
            else
                _listener_poller.poll(std::chrono::milliseconds{0}, perr);
        }

        _poller.poll(millis);
    }
};

} // namespace netty
