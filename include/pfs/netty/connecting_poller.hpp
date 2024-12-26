////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
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
    mutable std::function<void(socket_id, error const &)> on_failure;
    mutable std::function<void(socket_id, connection_refused_reason reason)> connection_refused;
    mutable std::function<void(socket_id)> connected;

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
