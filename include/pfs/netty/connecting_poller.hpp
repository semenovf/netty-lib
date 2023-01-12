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
#include <map>

namespace netty {

template <typename Backend>
class connecting_poller
{
public:
    using native_socket_type = typename Backend::native_socket_type;

private:
    Backend _rep;
    std::map<native_socket_type, clock_type::time_point> _connecting_sockets;

public:
    mutable std::function<void(native_socket_type, std::string const &)> on_error;
    mutable std::function<void(native_socket_type)> connection_refused;
    mutable std::function<void(native_socket_type)> connected;

public:
    NETTY__EXPORT connecting_poller ();
    NETTY__EXPORT ~connecting_poller ();

    connecting_poller (connecting_poller const &) = delete;
    connecting_poller & operator = (connecting_poller const &) = delete;
    connecting_poller (connecting_poller &&) = delete;
    connecting_poller & operator = (connecting_poller &&) = delete;

    NETTY__EXPORT void add (native_socket_type sock, error * perr = nullptr);
    NETTY__EXPORT void remove (native_socket_type sock, error * perr = nullptr);
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);
    NETTY__EXPORT bool empty () const noexcept;
};

} // namespace netty
