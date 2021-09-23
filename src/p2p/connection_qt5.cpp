////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "connection_qt5.hpp"

namespace pfs {
namespace net {
namespace p2p {

template <>
connection<backend_enum::qt5>::connection (std::unique_ptr<backend> && p)
    : _p(std::move(p))
{}

template <>
connection<backend_enum::qt5>::~connection () = default;

}}} // namespace pfs::net::p2p

