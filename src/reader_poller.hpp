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
void reader_poller<Backend>::init ()
{
    on_failure = [] (socket_id, error const & err) {
        fmt::println(stderr, tr::_("ERROR: reader poller: {}"), err.what());
    };

    disconnected = [] (socket_id) {};
    ready_read = [] (socket_id) {};
}

template <typename Backend>
reader_poller<Backend>::~reader_poller () = default;

template <typename Backend>
void reader_poller<Backend>::add (socket_id sock, error * perr)
{
    _rep->add_socket(sock, perr);
}

template <typename Backend>
void reader_poller<Backend>::remove (socket_id sock, error * perr)
{
    _rep->remove_socket(sock, perr);
}

template <typename Backend>
bool reader_poller<Backend>::empty () const noexcept
{
    return _rep->empty();
}

} // namespace netty
