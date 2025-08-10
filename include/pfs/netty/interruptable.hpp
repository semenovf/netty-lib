////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// References:
//
// Changelog:
//      2025.08.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include <atomic>

NETTY__NAMESPACE_BEGIN

class interruptable
{
private:
    std::atomic_bool _interrupted {false};

public:
    void interrupt ()
    {
        _interrupted.store(true);
    }

    bool interrupted () const noexcept
    {
        return _interrupted.load();
    }

    void clear_interrupted ()
    {
        _interrupted.store(false);
    }
};

NETTY__NAMESPACE_END
