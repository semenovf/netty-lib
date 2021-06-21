////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.06.21 Initial version
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "exports.hpp"
#include "error.hpp"
#include <string>

namespace pfs {
namespace net {

DLL_API int mtu (std::string const & interface, std::error_code & ec);

}} // namespace pfs::net
