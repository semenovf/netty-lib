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
#include <functional>

namespace netty {

template <typename Backend>
class reader_poller
{
public:
    using native_socket_type = typename Backend::native_socket_type;

private:
    Backend _rep;

public:
    mutable std::function<void(native_socket_type, error const &)> on_failure;
    mutable std::function<void(native_socket_type)> disconnected;
    mutable std::function<void(native_socket_type)> ready_read;

protected:
    struct specialized {};

    // Specialized poller
    reader_poller (specialized);

public:
    NETTY__EXPORT reader_poller ();
    NETTY__EXPORT ~reader_poller ();

    reader_poller (reader_poller const &) = delete;
    reader_poller & operator = (reader_poller const &) = delete;
    reader_poller (reader_poller &&) = delete;
    reader_poller & operator = (reader_poller &&) = delete;

    NETTY__EXPORT void add (native_socket_type sock, error * perr = nullptr);
    NETTY__EXPORT void remove (native_socket_type sock, error * perr = nullptr);
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);
    NETTY__EXPORT bool empty () const noexcept;
};

} // namespace netty

