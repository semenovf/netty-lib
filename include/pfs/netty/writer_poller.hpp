////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "error.hpp"
#include "exports.hpp"
#include "namespace.hpp"
#include <chrono>
#include <functional>
#include <memory>

NETTY__NAMESPACE_BEGIN

template <typename Backend>
class writer_poller
{
public:
    using socket_id = typename Backend::socket_id;

private:
    std::unique_ptr<Backend> _rep;

public:
    mutable std::function<void(socket_id, error const &)> on_failure;
    mutable std::function<void(socket_id)> can_write;

public:
    NETTY__EXPORT writer_poller ();
    NETTY__EXPORT ~writer_poller ();

    writer_poller (writer_poller const &) = delete;
    writer_poller & operator = (writer_poller const &) = delete;
    writer_poller (writer_poller &&) = delete;
    writer_poller & operator = (writer_poller &&) = delete;

    NETTY__EXPORT void wait_for_write (socket_id sock, error * perr = nullptr);
    NETTY__EXPORT void remove (socket_id sock, error * perr = nullptr);
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);
    NETTY__EXPORT bool empty () const noexcept;
};

NETTY__NAMESPACE_END
