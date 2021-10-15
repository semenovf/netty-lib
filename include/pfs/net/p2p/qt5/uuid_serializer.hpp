
////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "uuid_serializer.hpp"
#include "pfs/uuid.hpp"
#include <QDataStream>

namespace pfs {
namespace net {
namespace p2p {
namespace qt5 {

QDataStream & operator << (QDataStream & out, pfs::uuid_t uuid)
{
    union __int128_as_32
    {
        __uint128_t v;
        std::uint32_t d[4];
    };

    __int128_as_32 u;
    u.v = uuid;

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    out << htonl(u.d[3]) << htonl(u.d[2]) << htonl(u.d[1]) << htonl(u.d[0]);
#else
    out << htonl(u.d[0]) << htonl(u.d[1]) << htonl(u.d[2]) << htonl(u.d[3]);
#endif

    return out;
}

QDataStream & operator >> (QDataStream & in, pfs::uuid_t & uuid)
{
    union __int128_as_32
    {
        __uint128_t v;
        std::uint32_t d[4];
    };

    __int128_as_32 u;

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    in >> u.d[3] >> u.d[2] >> u.d[1] >> u.d[0];
#else
    in >> u.d[0] >> u.d[1] >> u.d[2] >> u.d[3];
#endif

    u.d[0] = ntohl(u.d[0]);
    u.d[1] = ntohl(u.d[1]);
    u.d[2] = ntohl(u.d[2]);
    u.d[3] = ntohl(u.d[3]);

    uuid = u.v;

    return in;
}
}}}} // namespace pfs::net::p2p::qt5
