////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"

NETTY__NAMESPACE_BEGIN

enum class conn_status {
      failure     = -1
    , unreachable = -2
    , connected   =  0
    , connecting  =  1
    , deferred    =  2
};

NETTY__NAMESPACE_END
