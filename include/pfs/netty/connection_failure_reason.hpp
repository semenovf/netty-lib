////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.12.26 Initial version.
//      2025.06.22 `connection_refused_reason` renamed to `connection_failure_reason`.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <string>

namespace netty {

enum class connection_failure_reason
{
      refused     // Connection refused by peer
    , timeout     // Connection timed out
    , reset       // Connection reset by peer
    , unreachable // No rote to host / host down
};

inline std::string to_string (connection_failure_reason reason)
{
    switch (reason) {
        case connection_failure_reason::refused: return "refused";
        case connection_failure_reason::timeout: return "timed out";
        case connection_failure_reason::reset: return "reset by peer";
        case connection_failure_reason::unreachable: return "unreachable";
        default:
            break;
    }

    return "other";
}

} // namespace netty
