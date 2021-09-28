////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "connection_qt5.hpp"
#include "pfs/memory.hpp"

namespace pfs {
namespace net {
namespace p2p {

template <>
connection<backend_enum::qt5>::connection ()
    : _p(pfs::make_unique<backend>(*this))
{}

template <>
connection<backend_enum::qt5>::connection (connection &&) = default;

template <>
connection<backend_enum::qt5>::~connection ()
{
    connected.disconnect_all();
    disconnected.disconnect_all();
    failure.disconnect_all();
}

template <>
connection<backend_enum::qt5> & connection<backend_enum::qt5>::operator = (connection &&) = default;

template <>
void connection<backend_enum::qt5>::connect (inet4_addr const & addr, std::uint16_t port)
{
    _p->connect(addr, port);
}

}}} // namespace pfs::net::p2p

