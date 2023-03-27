////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "local_file.hpp"
#include "remote_file.hpp"
#include "pfs/variant.hpp"

namespace netty {
namespace p2p {

using unified_file = pfs::variant<local_file, remote_file>;

}} // namespace netty::p2p

