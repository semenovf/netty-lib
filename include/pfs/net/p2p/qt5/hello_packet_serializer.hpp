////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../hello_packet.hpp"
#include "uuid_serializer.hpp"
#include <QByteArray>
#include <QDataStream>
#include <utility>

namespace pfs {
namespace net {
namespace p2p {
namespace qt5 {

inline QByteArray serialize (hello_packet const & packet)
{
    QByteArray result;
    QDataStream ds {& result, QIODevice::WriteOnly};
    ds.setByteOrder(QDataStream::BigEndian);

    ds.writeRawData(packet.greeting, sizeof(packet.greeting));
    ds << packet.uuid << packet.port << packet.crc32;

    return result;
}

inline std::pair<std::string, hello_packet> deserialize_hello (QByteArray const & data)
{
    using result_type = std::pair<std::string, hello_packet>;

    hello_packet packet;
    QDataStream ds {data};
    ds.setByteOrder(QDataStream::BigEndian);

    ds.readRawData(packet.greeting, sizeof(packet.greeting));
    ds >> packet.uuid >> packet.port >> packet.crc32;

    if (!(packet.greeting[0] == 'H'
            && packet.greeting[1] == 'E'
            && packet.greeting[2] == 'L'
            && packet.greeting[3] == 'O')) {

        return std::make_pair("bad hello greeting", hello_packet{});
    }

    auto crc32 = pfs::crc32_of_ptr(packet.greeting, sizeof(packet.greeting), 0);
    crc32 = pfs::crc32_all_of(crc32, packet.uuid, packet.port);

    if (crc32 != packet.crc32)
        return std::make_pair("bad CRC32", hello_packet{});

    return std::make_pair(std::string{}, std::move(packet));
}

}}}} // namespace pfs::net::p2p::qt5

