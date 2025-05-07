////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.12.26 Initial version.
//      2025.05.07 Replaced `std::function` with `callback_t`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include "callback.hpp"
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

public:
    mutable callback_t<void(error const &)> on_failure = [] (error const &) {};

    /**
     * Accept an incoming connection callback.
     */
    mutable callback_t<void(socket_type &&)> on_accepted = [] (socket_type &&) {};

public:
    listener_pool ()
    {
        ListenerPoller::on_failure = [this] (listener_id id, error const & err) {
            remove_later(id);
            this->on_failure(err);
        };

        ListenerPoller::accept = [this] (listener_id id) {
            error err;
            auto pos = _listeners.find(id);

            if (pos != _listeners.end()) {
                auto peer_socket = pos->second.accept_nonblocking(id, & err);

                if (!err)
                    this->on_accepted(std::move(peer_socket));
            } else {
                err = error {tr::f_("listener not found: {}", id)};
            }

            if (err)
                this->on_failure(err);
        };
    }

public:
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
                this->on_failure(err);
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
