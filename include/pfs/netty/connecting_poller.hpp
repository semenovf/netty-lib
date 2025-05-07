////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.09 Initial version.
//      2025.05.07 Replaced `std::function` with `callback_t`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "callback.hpp"
#include "connection_refused_reason.hpp"
#include "error.hpp"
#include "exports.hpp"
#include "namespace.hpp"
#include <chrono>
#include <functional>
#include <memory>

NETTY__NAMESPACE_BEGIN

template <typename Backend>
class connecting_poller
{
public:
    using socket_id = typename Backend::socket_id;

private:
    std::unique_ptr<Backend> _rep;

public:
    mutable callback_t<void(socket_id, error const &)> on_failure;
    mutable callback_t<void(socket_id, connection_refused_reason reason)> connection_refused;
    mutable callback_t<void(socket_id)> connected;

public:
    NETTY__EXPORT connecting_poller ();
    NETTY__EXPORT ~connecting_poller ();

    connecting_poller (connecting_poller const &) = delete;
    connecting_poller & operator = (connecting_poller const &) = delete;
    connecting_poller (connecting_poller &&) = delete;
    connecting_poller & operator = (connecting_poller &&) = delete;

    NETTY__EXPORT void add (socket_id sock, error * perr = nullptr);
    NETTY__EXPORT void remove (socket_id sock, error * perr = nullptr);

    /**
     * @return Number of connected sockets.
     */
    NETTY__EXPORT int poll (std::chrono::milliseconds millis = std::chrono::milliseconds{0}
        , error * perr = nullptr);

    NETTY__EXPORT bool empty () const noexcept;
};

NETTY__NAMESPACE_END
