////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.12.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "error.hpp"
#include <pfs/i18n.hpp>
#include <chrono>
#include <functional>
#include <map>

namespace netty {

template <typename ListenerPoller, typename ListenerSocket, typename Socket>
class listener_pool: protected ListenerPoller
{
public:
    using listener_socket_type = ListenerSocket;
    using socket_type = Socket;
    using socket_id = typename Socket::socket_id;
    using listener_id = typename ListenerSocket::listener_id;

private:
    std::map<listener_id, listener_socket_type> _listeners;
    std::vector<listener_id> _removable;
    mutable std::function<void(error const &)> _on_failure;
    mutable std::function<void(socket_type &&)> _on_accepted;

    void remove_later (socket_id id)
    {
        _removable.push_back(id);
    }

    void apply_remove ()
    {
        for (auto id: _removable) {
            ListenerPoller::remove(id);
            _listeners.erase(id);
        }

        _removable.clear();
    }

public:
    /**
     * Sets a callback for the failure. Callback signature is void(netty::error const &).
     */
    template <typename F>
    listener_pool & on_failure (F && f)
    {
        _on_failure = std::forward<F>(f);

        ListenerPoller::on_failure = [this] (listener_id id, error const & err) {
            remove_later(id);
            _on_failure(err);
        };

        return *this;
    }

    /**
     * Sets a callback to accept an incoming connection.
     * Callback signature is void(socket_type &&).
     */
    template <typename F>
    listener_pool & on_accepted (F && f)
    {
        _on_accepted = std::forward<F>(f);

        ListenerPoller::accept = [this] (listener_id id) {
            error err;
            auto pos = _listeners.find(id);

            if (pos != _listeners.end()) {
                auto peer_socket = pos->second.accept_nonblocking(& err);

                if (!err)
                    _on_accepted(std::move(peer_socket));
            } else {
                err = error {errc::device_not_found, tr::f_("listener not found by id: {}", id)};
            }

            if (err)
                _on_failure(err);
        };

        return *this;
    }

    template <typename ...Args>
    void add (Args &&... args)
    {
        ListenerSocket listener (std::forward<Args>(args)...);

        if (listener)
            _listeners[listener.id()] = std::move(listener);
    }

    void listen (int backlog)
    {
        for (auto & x: _listeners) {
            error err;
            auto & listener = x.second;
            auto success = listener.listen(backlog, & err);

            if (success)
                ListenerPoller::add(listener.id(), & err);

            if (err)
                _on_failure(err);
        }
    }

    /**
     * @return Number of pending connections, or negative value on error.
     */
    int step (std::chrono::milliseconds millis = std::chrono::milliseconds{0}
        , error * perr = nullptr)
    {
        if (!_removable.empty())
            apply_remove();

        return ListenerPoller::poll(millis, perr);
    }

    bool empty () const noexcept
    {
        return _listeners.empty();
    }
};

} // namespace netty

