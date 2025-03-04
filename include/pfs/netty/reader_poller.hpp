////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "chrono.hpp"
#include "error.hpp"
#include "exports.hpp"
#include "namespace.hpp"
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
    mutable std::function<void(socket_id, error const &)> on_failure;
    mutable std::function<void(socket_id)> on_disconnected;
    mutable std::function<void(socket_id)> on_ready_read;

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
