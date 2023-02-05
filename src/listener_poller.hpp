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
listener_poller<Backend>::listener_poller ()
    : listener_poller(specialized{})
{
    on_error = [] (native_socket_type, std::string const & text) {
        fmt::print(stderr, tr::_("ERROR: listener poller: {}\n"), text);
    };
}

template <typename Backend>
listener_poller<Backend>::~listener_poller () = default;

template <typename Backend>
void listener_poller<Backend>::add (native_socket_type sock, error * perr)
{
    _rep.add(sock, perr);
}

template <typename Backend>
void listener_poller<Backend>::remove (native_socket_type sock, error * perr)
{
    _rep.remove(sock, perr);
}

template <typename Backend>
bool listener_poller<Backend>::empty () const noexcept
{
    return _rep.empty();
}

} // namespace netty
