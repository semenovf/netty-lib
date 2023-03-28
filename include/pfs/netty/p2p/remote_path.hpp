////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/netty/socket4_addr.hpp"
#include <string>

namespace netty {
namespace p2p {

struct remote_path
{
    std::string uri;
    socket4_addr provider_saddr;

    bool empty () const noexcept
    {
        return uri.empty();
    }
};

}} // namespace netty::p2p
