////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2026 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2026.07.16 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../../namespace.hpp"
#include <pfs/universal_id.hpp>
#include <pfs/universal_id_pack.hpp>

NETTY__NAMESPACE_BEGIN

namespace meshnet {

using session_id_t = pfs::universal_id;

inline session_id_t generate_session_id ()
{
    return pfs::generate_uuid();
}

} // namespace meshnet

NETTY__NAMESPACE_END
