////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.24 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/i18n.hpp"
#include "pfs/netty/writer_poller.hpp"

namespace netty {

template <typename Backend>
void writer_poller<Backend>::init ()
{
    on_failure = [] (socket_id, error const & err) {
        fmt::println(stderr, tr::_("ERROR: writer poller: {}"), err.what());
    };

    can_write = [] (socket_id) {};
}

template <typename Backend>
writer_poller<Backend>::~writer_poller () = default;

template <typename Backend>
void writer_poller<Backend>::wait_for_write (socket_id sock, error * perr)
{
    _rep->wait_for_write(sock, perr);
}

template <typename Backend>
void writer_poller<Backend>::remove (socket_id sock, error * perr)
{
    _rep->remove_socket(sock, perr);
}

template <typename Backend>
bool writer_poller<Backend>::empty () const noexcept
{
    return _rep->empty();
}

} // namespace netty
