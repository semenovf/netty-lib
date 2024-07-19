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
#include "pfs/netty/listener_poller.hpp"

namespace netty {

template <typename Backend>
void listener_poller<Backend>::init ()
{
    on_failure = [] (native_listener_type, error const & err) {
        fmt::println(stderr, tr::_("ERROR: listener poller: {}"), err.what());
    };

    accept = [] (native_listener_type) {};
}

template <typename Backend>
listener_poller<Backend>::~listener_poller () = default;

template <typename Backend>
void listener_poller<Backend>::add (native_listener_type sock, error * perr)
{
    _rep->add_listener(sock, perr);
}

template <typename Backend>
void listener_poller<Backend>::remove (native_listener_type sock, error * perr)
{
    _rep->remove_listener(sock, perr);
}

template <typename Backend>
bool listener_poller<Backend>::empty () const noexcept
{
    return _rep->empty();
}

} // namespace netty
