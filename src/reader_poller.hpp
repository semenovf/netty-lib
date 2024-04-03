////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.23 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/i18n.hpp"
#include "pfs/netty/reader_poller.hpp"

namespace netty {

template <typename Backend>
reader_poller<Backend>::reader_poller ()
    : reader_poller(specialized{})
{
    on_failure = [] (native_socket_type, error const & err) {
        fmt::println(stderr, tr::_("ERROR: reader poller: {}"), err.what());
    };

    disconnected = [] (native_socket_type) {};
    ready_read = [] (native_socket_type) {};
}

template <typename Backend>
reader_poller<Backend>::~reader_poller () = default;

template <typename Backend>
void reader_poller<Backend>::add (native_socket_type sock, error * perr)
{
    _rep.add(sock, perr);
}

template <typename Backend>
void reader_poller<Backend>::remove (native_socket_type sock, error * perr)
{
    _rep.remove(sock, perr);
}

template <typename Backend>
bool reader_poller<Backend>::empty () const noexcept
{
    return _rep.empty();
}

} // namespace netty
