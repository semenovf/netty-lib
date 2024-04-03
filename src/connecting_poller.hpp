////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.11 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/i18n.hpp"
#include "pfs/netty/connecting_poller.hpp"

namespace netty {

template <typename Backend>
connecting_poller<Backend>::connecting_poller ()
    : connecting_poller(specialized{})
{
    on_failure = [] (native_socket_type, error const & err) {
        fmt::println(stderr, tr::_("ERROR: connecting poller: {}"), err.what());
    };

    connection_refused = [] (native_socket_type) {};
    connected = [] (native_socket_type) {};
}

template <typename Backend>
connecting_poller<Backend>::~connecting_poller () = default;

template <typename Backend>
void connecting_poller<Backend>::add (native_socket_type sock, error * perr)
{
    _rep.add(sock, perr);
}

template <typename Backend>
void connecting_poller<Backend>::remove (native_socket_type sock, error * perr)
{
    _rep.remove(sock, perr);
}

template <typename Backend>
bool connecting_poller<Backend>::empty () const noexcept
{
    return _rep.empty();
}

} // namespace netty
