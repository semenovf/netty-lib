////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.02.04 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/namespace.hpp>
#include <string>

NETTY__NAMESPACE_BEGIN

namespace patterns {

class without_logger
{
public:
    void log_debug (std::string const & msg) {}
    void log_info (std::string const & msg)  {}
    void log_warn (std::string const & msg)  {}
    void log_error (std::string const & msg) {}
};

} // namespace patterns

NETTY__NAMESPACE_END
