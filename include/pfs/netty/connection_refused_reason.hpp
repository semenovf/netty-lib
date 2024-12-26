////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.12.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <string>

namespace netty {

enum class connection_refused_reason
{
      other
    , timeout // Connection timed out
    , reset   // Connection reset by peer
};

inline std::string to_string (connection_refused_reason reason)
{
    switch (reason) {
        case connection_refused_reason::timeout: return "timed out";
        case connection_refused_reason::reset: return "reset by peer";
        case connection_refused_reason::other:
        default:
            break;
    }

    return "other";
}

} // namespace netty
