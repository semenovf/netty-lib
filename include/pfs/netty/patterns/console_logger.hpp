////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.01.25 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <pfs/netty/namespace.hpp>
#include <pfs/log.hpp>

NETTY__NAMESPACE_BEGIN

namespace patterns {

class console_logger
{
public:
    void log_debug (std::string const & msg) { LOGD("[meshnet]", "{}", msg); }
    void log_info (std::string const & msg)  { LOGI("[meshnet]", "{}", msg); }
    void log_warn (std::string const & msg)  { LOGW("[meshnet]", "{}", msg); }
    void log_error (std::string const & msg) { LOGE("[meshnet]", "{}", msg); }
};

} // namespace patterns

NETTY__NAMESPACE_END
