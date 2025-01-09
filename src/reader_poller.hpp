////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.23 Initial version.
//      2025.01.09 Removed init() method.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "netty/namespace.hpp"
#include "netty/reader_poller.hpp"
#include <pfs/i18n.hpp>

NETTY__NAMESPACE_BEGIN

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

NETTY__NAMESPACE_END
