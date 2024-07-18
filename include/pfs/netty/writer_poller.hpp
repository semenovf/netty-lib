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
#include <chrono>
#include <functional>
#include <memory>

namespace netty {

template <typename Backend>
class writer_poller
{
public:
    using native_socket_type = typename Backend::native_socket_type;

private:
    std::shared_ptr<Backend> _rep;

public:
    mutable std::function<void(native_socket_type, error const &)> on_failure;
    mutable std::function<void(native_socket_type)> can_write;

protected:
    void init ();

public:
    NETTY__EXPORT writer_poller (std::shared_ptr<Backend> backend = nullptr);
    NETTY__EXPORT ~writer_poller ();

    writer_poller (writer_poller const &) = delete;
    writer_poller & operator = (writer_poller const &) = delete;
    writer_poller (writer_poller &&) = delete;
    writer_poller & operator = (writer_poller &&) = delete;

    NETTY__EXPORT void add (native_socket_type sock, error * perr = nullptr);
    NETTY__EXPORT void remove (native_socket_type sock, error * perr = nullptr);
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);
    NETTY__EXPORT bool empty () const noexcept;
};

} // namespace netty
