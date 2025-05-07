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
#include "chrono.hpp"
#include "error.hpp"
#include "exports.hpp"
#include "namespace.hpp"
#include <functional>
#include <memory>

NETTY__NAMESPACE_BEGIN

template <typename Backend>
class listener_poller
{
public:
    using socket_id = typename Backend::socket_id;
    using listener_id = typename Backend::listener_id;

private:
    std::unique_ptr<Backend> _rep;

public:
    mutable callback_t<void(listener_id, error const &)> on_failure;

    /**
     * This callback must implement the accept procedure.
     */
    mutable callback_t<void(listener_id)> accept;

public:
    NETTY__EXPORT listener_poller ();
    NETTY__EXPORT ~listener_poller ();

    listener_poller (listener_poller const &) = delete;
    listener_poller & operator = (listener_poller const &) = delete;
    listener_poller (listener_poller &&) = delete;
    listener_poller & operator = (listener_poller &&) = delete;

    NETTY__EXPORT void add (listener_id sock, error * perr = nullptr);
    NETTY__EXPORT void remove (listener_id sock, error * perr = nullptr);

    /**
     * @resturn Number of pending connections, or negative value on error.
     */
    NETTY__EXPORT int poll (std::chrono::milliseconds millis = std::chrono::milliseconds{0}
        , error * perr = nullptr);

    NETTY__EXPORT bool empty () const noexcept;
};

NETTY__NAMESPACE_END
