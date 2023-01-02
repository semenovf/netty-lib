////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/error.hpp"
#include "pfs/netty/exports.hpp"
#include <chrono>
#include <functional>
// #include <set>
// #include <string>

namespace netty {

template <typename Backend>
class poller
{
public:
    using native_type = typename Backend::native_type;

private:
    Backend _rep;

public:
    mutable std::function<void(native_type)> on_error = [] (native_type) {};
    mutable std::function<void(native_type)> disconnected = [] (native_type) {};
    mutable std::function<void(native_type)> ready_read = [] (native_type) {};
    mutable std::function<void(native_type)> can_write = [] (native_type) {};
    mutable std::function<void(native_type, int)> unsupported_event = [] (native_type, int) {};

public:
    NETTY__EXPORT poller ();
    NETTY__EXPORT ~poller ();

    poller (poller const &) = delete;
    poller & operator = (poller const &) = delete;

    NETTY__EXPORT poller (poller &&);
    NETTY__EXPORT poller & operator = (poller &&);

    NETTY__EXPORT void add (native_type sock, error * perr = nullptr);
    NETTY__EXPORT void remove (native_type sock, error * perr = nullptr);
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);

    NETTY__EXPORT bool empty () const noexcept;
};

} // namespace netty

