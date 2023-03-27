////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.03.27 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "remote_path.hpp"
#include "pfs/ionik/file.hpp"
#include "pfs/ionik/file_provider.hpp"
#include <memory>

namespace netty {
namespace p2p {

class remote_file_handle;

using remote_file_provider = ionik::file_provider<std::unique_ptr<remote_file_handle>
    , remote_path>;
using remote_file = ionik::file<remote_file_provider>;

}} // namespace netty::p2p
