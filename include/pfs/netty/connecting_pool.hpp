////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.12.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "connection_refused_reason.hpp"
#include "error.hpp"
#include "namespace.hpp"
#include <pfs/i18n.hpp>
#include <chrono>
#include <functional>
#include <map>
#include <vector>

NETTY__NAMESPACE_BEGIN

template <typename ConnectingPoller, typename Socket>
class connecting_pool: protected ConnectingPoller
{
public:
    using socket_type = Socket;
    using socket_id = typename Socket::socket_id;

private:
    std::map<socket_id, socket_type> _connecting_sockets;
    std::vector<socket_id> _removable;
    mutable std::function<void(error const &)> _on_failure;
    mutable std::function<void(socket_type &&)> _on_connected;
    mutable std::function<void(socket_type &&, connection_refused_reason)> _on_connection_refused;

private:
    void remove_later (socket_id id)
    {
        _removable.push_back(id);
    }

    void apply_remove ()
    {
        for (auto id: _removable) {
            ConnectingPoller::remove(id);
            _connecting_sockets.erase(id);
        }

        _removable.clear();
    }

public:
    /**
     * Sets a callback for the failure. Callback signature is void(netty::error const &).
     */
    template <typename F>
    connecting_pool & on_failure (F && f)
    {
        _on_failure = std::forward<F>(f);

        ConnectingPoller::on_failure = [this] (socket_id id, error const & err) {
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
    connecting_pool & on_connected (F && f)
    {
        _on_connected = std::forward<F>(f);

        ConnectingPoller::connected = [this] (socket_id id) {
            error err;
            auto pos = _connecting_sockets.find(id);

            if (pos != _connecting_sockets.end()) {
                remove_later(id);
                _on_connected(std::move(pos->second));
            } else {
                err = error {errc::device_not_found, tr::f_("on_connected: socket not found by id: {}", id)};
            }

            if (err)
                _on_failure(err);
        };

        return *this;
    }

    template <typename F>
    connecting_pool & on_connection_refused (F && f)
    {
        _on_connection_refused = std::forward<F>(f);

        ConnectingPoller::connection_refused = [this] (socket_id id, connection_refused_reason reason) {
            error err;
            auto pos = _connecting_sockets.find(id);

            if (pos != _connecting_sockets.end()) {
                remove_later(id);
                _on_connection_refused(std::move(pos->second), reason);
            } else {
                err = error {errc::device_not_found, tr::f_("on_connection_refused: socket not found by id: {}", id)};
            }

            if (err)
                _on_failure(err);
        };

        return *this;
    }

    template <typename ...Args>
    netty::conn_status connect (Args &&... args)
    {
        Socket sock;
        error err;
        auto status = sock.connect(std::forward<Args>(args)..., & err);

        switch (status) {
            case netty::conn_status::connected:
                _on_connected(std::move(sock));
                break;
            case netty::conn_status::connecting: {
                ConnectingPoller::add(sock.id(), & err);

                if (!err) {
                    _connecting_sockets[sock.id()] = std::move(sock);
                } else {
                    _on_failure(err);
                }

                break;
            }

            case netty::conn_status::failure:
                _on_failure(err);
                break;
        }

        return status;
    }

    /**
     * @resturn Number of pending connections, or negative value on error.
     */
    int poll (std::chrono::milliseconds millis = std::chrono::milliseconds{0}, error * perr = nullptr)
    {
        apply_remove();
        return ConnectingPoller::poll(millis, perr);
    }

    bool empty () const noexcept
    {
        return _connecting_sockets.empty();
    }
};

NETTY__NAMESPACE_END
