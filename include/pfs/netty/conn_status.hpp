////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.26 Initial version.
//      2023.01.06 Renamed to `udt_socket` and refactored.
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace netty {

enum class conn_status {
      failure     = -1
    , connected   =  0
    , connecting  =  1
};

} // namespace netty
