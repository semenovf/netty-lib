////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.11 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "netty/listener_poller.hpp"
#include <pfs/log.hpp>

NETTY__NAMESPACE_BEGIN

template <typename Backend>
listener_poller<Backend>::~listener_poller () = default;

template <typename Backend>
void listener_poller<Backend>::add (listener_id sock, error * perr)
{
    _rep->add_listener(sock, perr);
}

template <typename Backend>
void listener_poller<Backend>::remove (listener_id sock, error * perr)
{
    _rep->remove_listener(sock, perr);
}

template <typename Backend>
bool listener_poller<Backend>::empty () const noexcept
{
    return _rep->empty();
}

NETTY__NAMESPACE_END
