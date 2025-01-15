////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.11 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "netty/connecting_poller.hpp"
#include <pfs/i18n.hpp>

NETTY__NAMESPACE_BEGIN

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

NETTY__NAMESPACE_END
