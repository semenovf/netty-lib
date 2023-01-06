////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "error.hpp"
#include "exports.hpp"
#include "posix/tcp_server.hpp"
#include "posix/tcp_socket.hpp"
#include "posix/udp_socket.hpp"
#include <chrono>
#include <functional>
#include <map>

namespace netty {

enum class ready_read_flag {
      good = 0
    , disconnected
    , check_disconnected
};

template <typename Backend>
class poller
{
public:
    using native_socket_type = typename Backend::native_socket_type;

private:
    Backend _rep;

public:
    mutable std::function<void(native_socket_type, std::string const &)> on_error;
    mutable std::function<void(native_socket_type)> connection_refused;
    mutable std::function<void(native_socket_type, ready_read_flag flag)> ready_read;
    mutable std::function<void(native_socket_type)> can_write;

public:
    NETTY__EXPORT poller ();
    NETTY__EXPORT ~poller ();

    poller (poller const &) = delete;
    poller & operator = (poller const &) = delete;
    poller (poller &&) = delete;
    poller & operator = (poller &&) = delete;

    NETTY__EXPORT void add (native_socket_type sock, error * perr = nullptr);
    NETTY__EXPORT void remove (native_socket_type sock, error * perr = nullptr);
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);
    NETTY__EXPORT bool empty () const noexcept;
};

template <typename Backend>
struct server_poller_callbacks
{
    using native_socket_type = typename Backend::native_socket_type;

    std::function<void(native_socket_type, std::string const &)> on_error;
    std::function<void(posix::tcp_socket &&)> accepted;
    std::function<void(native_socket_type)> ready_read;
    std::function<void(native_socket_type)> can_write;
};

template <typename Backend>
class server_poller
{
public:
    using native_socket_type = typename Backend::native_socket_type;
    using callbacks_type = server_poller_callbacks<Backend>;

private:
    poller<Backend> _listener_poller;
    poller<Backend> _client_poller;
    std::map<posix::tcp_server::native_type, posix::tcp_server *> _servers;

private: // callbacks
    std::function<void(posix::tcp_socket &&)> accepted;
    std::function<void(native_socket_type)> ready_read;

public:
    NETTY__EXPORT server_poller (callbacks_type && callbacks);
    NETTY__EXPORT ~server_poller ();

    server_poller (server_poller const &) = delete;
    server_poller & operator = (server_poller const &) = delete;
    server_poller (server_poller &&) = delete;
    server_poller & operator = (server_poller &&) = delete;

    NETTY__EXPORT void add (posix::tcp_server & server, error * perr = nullptr);
    NETTY__EXPORT void remove (posix::tcp_server const & server, error * perr = nullptr);
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);
};

template <typename Backend>
struct client_poller_callbacks
{
    using native_socket_type = typename Backend::native_socket_type;

    std::function<void(native_socket_type, std::string const &)> on_error;
    std::function<void(native_socket_type)> connection_refused;
    std::function<void(native_socket_type)> connected;
    std::function<void(native_socket_type)> disconnected;
    std::function<void(native_socket_type)> ready_read;
    std::function<void(native_socket_type)> can_write;
};

template <typename Backend>
class client_poller
{
public:
    using native_socket_type = typename Backend::native_socket_type;
    using callbacks_type = client_poller_callbacks<Backend>;

private:
    poller<Backend> _connecting_poller;
    poller<Backend> _regular_poller;

private:
    // Called when the socket is connected to the server.
    std::function<void(native_socket_type)> connected;

    // Called when local socket was shutted down or remote peer was closed
    // (server stoppped)
    std::function<void(native_socket_type)> disconnected;

    // Called when data received
    std::function<void(native_socket_type)> ready_read;

public:
    NETTY__EXPORT client_poller (callbacks_type && callbacks);
    NETTY__EXPORT ~client_poller ();

    client_poller (client_poller const &) = delete;
    client_poller & operator = (client_poller const &) = delete;
    client_poller (client_poller &&) = delete;
    client_poller & operator = (client_poller &&) = delete;

    NETTY__EXPORT void add (posix::tcp_socket & sock, error * perr = nullptr);
    NETTY__EXPORT void add (posix::udp_socket & sock, error * perr = nullptr);
    NETTY__EXPORT void remove (posix::tcp_socket const & sock, error * perr = nullptr);
    NETTY__EXPORT void remove (posix::udp_socket const & sock, error * perr = nullptr);
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);
};

} // namespace netty
