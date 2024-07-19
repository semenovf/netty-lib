////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "chrono.hpp"
#include "error.hpp"
#include "exports.hpp"
#include <functional>
#include <memory>

namespace netty {

template <typename Backend>
class listener_poller
{
public:
    using native_socket_type = typename Backend::native_socket_type;
    using native_listener_type = typename Backend::native_listener_type;

private:
    std::shared_ptr<Backend> _rep;

public:
    mutable std::function<void(native_listener_type, error const &)> on_failure;
    mutable std::function<void(native_listener_type)> accept;

protected:
    void init ();

public:
    NETTY__EXPORT listener_poller (std::shared_ptr<Backend> backend = nullptr);
    NETTY__EXPORT ~listener_poller ();

    listener_poller (listener_poller const &) = delete;
    listener_poller & operator = (listener_poller const &) = delete;
    listener_poller (listener_poller &&) = delete;
    listener_poller & operator = (listener_poller &&) = delete;

    NETTY__EXPORT void add (native_listener_type sock, error * perr = nullptr);
    NETTY__EXPORT void remove (native_listener_type sock, error * perr = nullptr);

    /**
     * @resturn Number of pending connections, or negative value on error.
     */
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);

    NETTY__EXPORT bool empty () const noexcept;
};

} // namespace netty
