////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"

NETTY__NAMESPACE_BEGIN

#if NETTY__TRACE_ENABLED
#   define NETTY__TRACE(x) x
#else
#   define NETTY__TRACE(x)
#endif

NETTY__NAMESPACE_END
