////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.06 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "exports.hpp"

namespace netty {

extern "C" NETTY__EXPORT void startup ();
extern "C" NETTY__EXPORT void cleanup ();

class startup_guard
{
public:
    startup_guard () { startup(); }
    ~startup_guard () { cleanup(); }
};

} // namespace netty
