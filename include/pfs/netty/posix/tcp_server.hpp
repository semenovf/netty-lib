////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
//      2024.05.14 Deprecated.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "tcp_listener.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/error.hpp"

namespace netty {
namespace posix {

/**
 * @deprecated For backward compatibility, use tcp_listener instead.
 */
using tcp_server = tcp_listener;

}} // namespace netty::posix
