////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.10.26 Initial version.
//      2023.01.24 Renamed to `udt_socket` and refactored.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <ios>

namespace netty {

enum class send_status {
      failure  = -1
    , good     =  0
    , again    =  1
    , overflow =  2
};

struct send_result
{
    send_status state;
    std::streamsize n;
};

} // namespace netty
