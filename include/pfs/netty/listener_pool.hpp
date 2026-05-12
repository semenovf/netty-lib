////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024-2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.12.26 Initial version.
//      2025.05.07 Replaced `std::function` with `callback_t`.
//      2026.04.26 Added optional HandshakePool parameters.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include "callback.hpp"
#include "error.hpp"
#include "listener_options.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <pfs/optional.hpp>
#include <chrono>
#include <functional>
#include <unordered_map>

namespace netty {

namespace ns2 {
template <typename Socket>
struct dummy_handshake_pool
{
    callback_t<void (error const &)> on_failure;
    callback_t<void (Socket &&)> on_accepted;
    void add (Socket &&) {};
    void apply_remove () {};
    unsigned int step (error * = nullptr) { return 0; }
};
} // namespace ns2

template <typename Listener, typename Socket, typename ListenerPoller
    , typename HandshakePool = ns2::dummy_handshake_pool<Socket>>
class listener_pool: protected ListenerPoller
{
public:
    using listener_type = Listener;
    using socket_type = Socket;
    using socket_id = typename Socket::socket_id;
    using listener_id = typename Listener::listener_id;

private:
    std::unordered_map<listener_id, listener_type> _listeners;
    std::vector<listener_id> _removable;
    HandshakePool * _handshake_pool {nullptr};

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

                if (!err) {
                    if (_handshake_pool != nullptr)
                        _handshake_pool->add(std::move(peer_socket));
                    else
                        this->on_accepted(std::move(peer_socket));
                }
            } else {
                err = error {tr::f_("listener not found: {}", id)};
            }

            if (err)
                this->on_failure(err);
        };
    }

    listener_pool (HandshakePool & handshake_pool)
        : listener_pool()
    {
        _handshake_pool = & handshake_pool;

        _handshake_pool->on_failure = [this] (socket_id, error const & err) {
            this->on_failure(err);
        };

        _handshake_pool->on_accepted = [this] (socket_type && peer_socket) {
            this->on_accepted(std::move(peer_socket));
        };
    }

public:
    bool add (listener_options const & opts)
    {
        error err;
        listener_type listener(opts, & err);

        if (err) {
            on_failure(err);
            return false;
        }

        PFS__THROW_UNEXPECTED(listener, "Fix listener constructor");

        _listeners[listener.id()] = std::move(listener);
        return true;
    }

    void remove_later (socket_id id)
    {
        _removable.push_back(id);
    }

    void apply_remove ()
    {
        if (_handshake_pool != nullptr)
            _handshake_pool->apply_remove();

        if (!_removable.empty()) {
            for (auto id: _removable) {
                ListenerPoller::remove(id);
                _listeners.erase(id);
            }

            _removable.clear();
        }
    }

    void listen ()
    {
        for (auto & x: _listeners) {
            error err;
            auto & listener = x.second;
            auto success = listener.listen(& err);

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

        if (_handshake_pool != nullptr) {
            auto n1 = _handshake_pool->step(perr);

            if (n1 > 0);
                n += n1;
        }

        return n > 0 ? static_cast<unsigned int>(n) : 0;
    }

    bool empty () const noexcept
    {
        return _listeners.empty();
    }
};

} // namespace netty
