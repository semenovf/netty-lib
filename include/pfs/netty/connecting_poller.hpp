////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "error.hpp"
#include "exports.hpp"
#include <functional>
#include <memory>

namespace netty {

template <typename Backend>
class connecting_poller
{
public:
    using socket_id = typename Backend::socket_id;

private:
    std::shared_ptr<Backend> _rep;

public:
    mutable std::function<void(socket_id, error const &)> on_failure;
    mutable std::function<void(socket_id, bool timedout)> connection_refused;
    mutable std::function<void(socket_id)> connected;

protected:
    void init ();

public:
    NETTY__EXPORT connecting_poller (std::shared_ptr<Backend> backend = nullptr);
    NETTY__EXPORT ~connecting_poller ();

    connecting_poller (connecting_poller const &) = delete;
    connecting_poller & operator = (connecting_poller const &) = delete;
    connecting_poller (connecting_poller &&) = delete;
    connecting_poller & operator = (connecting_poller &&) = delete;

    NETTY__EXPORT void add (socket_id sock, error * perr = nullptr);
    NETTY__EXPORT void remove (socket_id sock, error * perr = nullptr);

    /**
     * @return Number of connected sockets.
     */
    NETTY__EXPORT int poll (std::chrono::milliseconds millis, error * perr = nullptr);

    NETTY__EXPORT bool empty () const noexcept;
};

} // namespace netty
