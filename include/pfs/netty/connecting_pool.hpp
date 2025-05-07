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
#include "connection_refused_reason.hpp"
#include "error.hpp"
#include "namespace.hpp"
#include <pfs/i18n.hpp>
#include <chrono>
#include <functional>
#include <map>
#include <set>
#include <utility>
#include <vector>

NETTY__NAMESPACE_BEGIN

template <typename Socket, typename ConnectingPoller>
class connecting_pool: protected ConnectingPoller
{
public:
    using socket_type = Socket;
    using socket_id = typename Socket::socket_id;

private:
    using time_point_type = std::chrono::steady_clock::time_point;

    struct deferred_connection_item
    {
        time_point_type t;
        std::function<void()> f;
    };

    friend constexpr bool operator < (deferred_connection_item const & lhs, deferred_connection_item const & rhs)
    {
        return lhs.t < rhs.t;
    }

private:
    std::map<socket_id, socket_type> _connecting_sockets;
    std::vector<socket_id> _removable;
    std::set<deferred_connection_item> _deferred_connections;

public:
    mutable callback_t<void (error const &)> on_failure = [] (error const &) {};
    mutable callback_t<void (socket_type &&)> on_connected = [] (socket_type &&) {};
    mutable callback_t<void (socket4_addr, connection_refused_reason)> on_connection_refused
        = [] (socket4_addr, connection_refused_reason) {};

public:
    connecting_pool ()
    {
        ConnectingPoller::on_failure = [this] (socket_id id, error const & err) {
            remove_later(id);
            this->on_failure(err);
        };

        ConnectingPoller::connected = [this] (socket_id id) {
            error err;
            auto pos = _connecting_sockets.find(id);
            remove_later(id);

            if (pos != _connecting_sockets.end()) {
                if (this->on_connected)
                    this->on_connected(std::move(pos->second));
            } else {
                err = error {tr::f_("on socket connected failure: id={}", id)};
            }

            if (err)
                this->on_failure(err);
        };

        ConnectingPoller::connection_refused = [this] (socket_id id, connection_refused_reason reason) {
            error err;
            auto pos = _connecting_sockets.find(id);
            remove_later(id);

            if (pos != _connecting_sockets.end()) {
                if (this->on_connection_refused)
                    this->on_connection_refused(pos->second.saddr(), reason);
            } else {
                this->on_failure(error {tr::f_("on connection refused on socket failure: id={}", id)});
            }
        };
    }

public:
    void remove_later (socket_id id)
    {
        _removable.push_back(id);
    }

    void apply_remove ()
    {
        if (!_removable.empty()) {
            for (auto id: _removable) {
                ConnectingPoller::remove(id);
                _connecting_sockets.erase(id);
            }

            _removable.clear();
        }
    }

    template <typename ...Args>
    netty::conn_status connect (Args &&... args)
    {
        Socket sock;
        error err;
        auto status = sock.connect(std::forward<Args>(args)..., & err);

        switch (status) {
            case netty::conn_status::connected:
                this->on_connected(std::move(sock));
                break;
            case netty::conn_status::connecting: {
                ConnectingPoller::add(sock.id(), & err);

                if (!err) {
                    _connecting_sockets[sock.id()] = std::move(sock);
                } else {
                    this->on_failure(err);
                }

                break;
            }

            case netty::conn_status::unreachable:
                this->on_connection_refused(sock.saddr(), connection_refused_reason::unreachable);
                break;

            case netty::conn_status::failure:
                this->on_failure(err);
                break;

            case netty::conn_status::deferred:
            default:
                break;
        }

        return status;
    }

    template <typename ...Args>
    netty::conn_status connect_timeout (std::chrono::milliseconds timeout, Args &&... args)
    {
        if (timeout <= std::chrono::milliseconds{0})
            return connect(std::forward<Args>(args)...);

        auto func = [this, args...] () {
            this->connect(args...);
        };

        _deferred_connections.insert(deferred_connection_item{std::chrono::steady_clock::now() + timeout, std::move(func)});

        return netty::conn_status::deferred;
    }

    /**
     * @return Number of events occurred.
     */
    unsigned int step (error * perr = nullptr)
    {
        // Reconnect
        if (!_deferred_connections.empty()) {
            auto now = std::chrono::steady_clock::now();
            auto pos = _deferred_connections.begin();

            while (!_deferred_connections.empty() && pos->t <= now) {
                pos->f();
                pos = _deferred_connections.erase(pos);
            }
        }

        auto n =ConnectingPoller::poll(std::chrono::milliseconds{0}, perr);
        return n > 0 ? static_cast<unsigned int>(n) : 0;
    }

    bool empty () const noexcept
    {
        return _connecting_sockets.empty();
    }
};

NETTY__NAMESPACE_END
