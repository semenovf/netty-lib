////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/assert.hpp"

namespace netty {

extern "C" void startup ();
extern "C" void cleanup ();

class startup_guard
{
public:
    startup_guard () { startup(); }
    ~startup_guard () { cleanup(); }
};

} // namespace netty

