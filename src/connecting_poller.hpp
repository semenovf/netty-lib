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
void connecting_poller<Backend>::init ()
{
    on_failure = [] (socket_id, error const & err) {
        fmt::println(stderr, tr::_("ERROR: connecting poller: {}"), err.what());
    };

    connection_refused = [] (socket_id, bool) {};
    connected = [] (socket_id) {};
}

template <typename Backend>
connecting_poller<Backend>::~connecting_poller () = default;

template <typename Backend>
void connecting_poller<Backend>::add (socket_id sock, error * perr)
{
    _rep->add_socket(sock, perr);
}

template <typename Backend>
void connecting_poller<Backend>::remove (socket_id sock, error * perr)
{
    _rep->remove_socket(sock, perr);
}

template <typename Backend>
bool connecting_poller<Backend>::empty () const noexcept
{
    return _rep->empty();
}

} // namespace netty
