////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2021.11.09 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/endian.hpp"
#include "pfs/universal_id.hpp"
#include "pfs/universal_id_crc.hpp"
#include "pfs/universal_id_hash.hpp"

namespace netty {
namespace p2p {

using universal_id = pfs::universal_id;
    /// @depricated Use `host_id` instead

}} // namespace netty::p2p
