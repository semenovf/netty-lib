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
#include <unordered_map>

namespace netty {

template <typename ListenerSocket, typename Socket, typename ListenerPoller>
class listener_pool: protected ListenerPoller
{
public:
    using listener_socket_type = ListenerSocket;
    using socket_type = Socket;
    using socket_id = typename Socket::socket_id;
    using listener_id = typename ListenerSocket::listener_id;

private:
    std::unordered_map<listener_id, listener_socket_type> _listeners;
    std::vector<listener_id> _removable;

    mutable std::function<void(error const &)> _on_failure = [] (error const &) {};
    mutable std::function<void(socket_type &&)> _on_accepted;

public:
    listener_pool ()
    {
        ListenerPoller::on_failure = [this] (listener_id id, error const & err) {
            remove_later(id);
            _on_failure(err);
        };

        ListenerPoller::accept = [this] (listener_id id) {
            error err;
            auto pos = _listeners.find(id);

            if (pos != _listeners.end()) {
                auto peer_socket = pos->second.accept_nonblocking(id, & err);

                if (!err)
                    _on_accepted(std::move(peer_socket));
            } else {
                err = error {tr::f_("listener not found: {}", id)};
            }

            if (err)
                _on_failure(err);
        };
    }

public:
    /**
     * Sets a callback for the failure. Callback signature is void(netty::error const &).
     */
    template <typename F>
    listener_pool & on_failure (F && f)
    {
        _on_failure = std::forward<F>(f);
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
        return *this;
    }

    template <typename ...Args>
    void add (Args &&... args)
    {
        ListenerSocket listener (std::forward<Args>(args)...);

        if (listener)
            _listeners[listener.id()] = std::move(listener);
    }

    void remove_later (socket_id id)
    {
        _removable.push_back(id);
    }

    void apply_remove ()
    {
        if (!_removable.empty()) {
            for (auto id: _removable) {
                ListenerPoller::remove(id);
                _listeners.erase(id);
            }

            _removable.clear();
        }
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
     * @return Number of events occurred.
     */
    unsigned int step (error * perr = nullptr)
    {
        auto n = ListenerPoller::poll(std::chrono::milliseconds{0}, perr);
        return n > 0 ? static_cast<unsigned int>(n) : 0;
    }

    bool empty () const noexcept
    {
        return _listeners.empty();
    }
};

} // namespace netty
