////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.06.21 Initial version.
//      2025.03.11 Refactored.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/error.hpp"

namespace netty {

class error: public pfs::error
{
public:
    using pfs::error::error;
};

} // namespace netty
