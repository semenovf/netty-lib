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
        //std::function<void(native_socket_type)> disconnected;
        std::function<void(native_socket_type)> ready_read;
        std::function<void(native_socket_type)> can_write;
    };

private:
    listener_poller<Backend> _listener_poller;
    regular_poller<Backend>  _poller;

private: // callbacks
    std::function<native_socket_type(native_socket_type, bool &)> accept;

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

        _listener_poller.accept = [this] (native_socket_type listener_sock) {
            bool ok = true;
            auto sock = this->accept(listener_sock, ok);

            if (ok)
                _poller.add(sock);
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
     * Remove client socket.
     */
    template <typename Socket>
    void remove (Socket const & sock, error * perr = nullptr)
    {
        _poller.remove(sock.native(), perr);
    }

    /**
     * Check if listener and regular pollers are empty.
     */
    bool empty () const noexcept
    {
        return _listener_poller.empty() && _poller.empty();
    }

    /**
     * @return Pair of poll results of listener and regular pollers.
     */
    std::pair<int, int> poll (std::chrono::milliseconds millis, error * perr = nullptr)
    {
        int n1 = 0;

        if (!_listener_poller.empty()) {
            if (_poller.empty())
                n1 = _listener_poller.poll(millis, perr);
            else
                n1 = _listener_poller.poll(std::chrono::milliseconds{0}, perr);
        }

        auto n2 = _poller.poll(millis);

        return std::make_pair(n1, n2);
    }
};

} // namespace netty
