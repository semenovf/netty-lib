////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.23 Initial version.
//      2025.05.07 Replaced `std::function` with `callback_t`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include "callback.hpp"
#include "chrono.hpp"
#include "error.hpp"
#include "exports.hpp"
#include <functional>
#include <memory>

NETTY__NAMESPACE_BEGIN

template <typename Backend>
class reader_poller
{
public:
    using socket_id = typename Backend::socket_id;

private:
    std::unique_ptr<Backend> _rep;

public:
    mutable std::function<void (socket_id, error const &)> on_failure = [] (socket_id, error const &) {};
    mutable std::function<void (socket_id)> on_disconnected = [] (socket_id) {};
    mutable std::function<void (socket_id)> on_ready_read = [] (socket_id) {};

public:
    NETTY__EXPORT reader_poller ();
    NETTY__EXPORT ~reader_poller ();

    reader_poller (reader_poller const &) = delete;
    reader_poller & operator = (reader_poller const &) = delete;
    reader_poller (reader_poller &&) = delete;
    reader_poller & operator = (reader_poller &&) = delete;

    NETTY__EXPORT void add (socket_id sock, error * perr = nullptr);
    NETTY__EXPORT void remove (socket_id sock, error * perr = nullptr);
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);
    NETTY__EXPORT bool empty () const noexcept;
};

NETTY__NAMESPACE_END
