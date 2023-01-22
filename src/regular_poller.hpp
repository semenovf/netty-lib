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
#include "pfs/netty/regular_poller.hpp"

namespace netty {

template <typename Backend>
regular_poller<Backend>::regular_poller ()
    : _rep()
{
    on_error = [] (native_socket_type, std::string const & text) {
        fmt::print(stderr, tr::f_("ERROR: regular poller: {}\n"), text);
    };
}

template <typename Backend>
regular_poller<Backend>::~regular_poller () = default;

template <typename Backend>
void regular_poller<Backend>::add (native_socket_type sock, error * perr)
{
    _rep.add(sock, perr);
}

template <typename Backend>
void regular_poller<Backend>::remove (native_socket_type sock, error * perr)
{
    _rep.remove(sock, perr);
}

template <typename Backend>
bool regular_poller<Backend>::empty () const noexcept
{
    return _rep.empty();
}

} // namespace netty
