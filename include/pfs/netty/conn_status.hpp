////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.01 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace netty {

enum class conn_status {
      failure     = -1
    , unreachable = -2
    , connected   =  0
    , connecting  =  1
    , deferred    =  2
};

} // namespace netty
