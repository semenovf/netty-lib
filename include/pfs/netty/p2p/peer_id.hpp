////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.04.26 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/universal_id.hpp"
#include "pfs/universal_id_crc.hpp"
#include "pfs/universal_id_hash.hpp"

namespace netty {
namespace p2p {

using peer_id = pfs::universal_id;

}} // namespace netty::p2p
