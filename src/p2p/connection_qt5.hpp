////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/net/p2p/connection.hpp"
#include <QTcpSocket>

namespace pfs {
namespace net {
namespace p2p {

template <>
class connection<backend_enum::qt5>::backend
{
    QTcpSocket * _peer {nullptr};

public:
    backend (QTcpSocket * peer)
        : _peer(peer)
    {}

    ~backend () {}
};

}}} // namespace pfs::net::p2p
