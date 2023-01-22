////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
//      2023.01.11 Renamed to `regular_poller`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "chrono.hpp"
#include "error.hpp"
#include "exports.hpp"
#include <functional>

namespace netty {

template <typename Backend>
class regular_poller
{
public:
    using native_socket_type = typename Backend::native_socket_type;

private:
    Backend _rep;

public:
    mutable std::function<void(native_socket_type, std::string const &)> on_error;
    mutable std::function<void(native_socket_type)> disconnected;
    mutable std::function<void(native_socket_type)> ready_read;
    mutable std::function<void(native_socket_type)> can_write;

public:
    NETTY__EXPORT regular_poller ();
    NETTY__EXPORT ~regular_poller ();

    regular_poller (regular_poller const &) = delete;
    regular_poller & operator = (regular_poller const &) = delete;
    regular_poller (regular_poller &&) = delete;
    regular_poller & operator = (regular_poller &&) = delete;

    NETTY__EXPORT void add (native_socket_type sock, error * perr = nullptr);
    NETTY__EXPORT void remove (native_socket_type sock, error * perr = nullptr);
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);
    NETTY__EXPORT bool empty () const noexcept;
};

} // namespace netty
