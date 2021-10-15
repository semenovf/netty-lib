////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.10 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../packet.hpp"
#include "uuid_serializer.hpp"
#include "pfs/uuid.hpp"
#include <QByteArray>
#include <QDataStream>
#include <utility>
#include <cassert>

namespace pfs {
namespace net {
namespace p2p {
namespace qt5 {

template <std::size_t PacketSize>
QByteArray serialize (packet<PacketSize> const & pkt)
{
    using packet_type = packet<PacketSize>;

    QByteArray result;
    QDataStream ds {result};
    ds.setByteOrder(QDataStream::BigEndian);

    ds << pkt.startflag
        << pkt.uuid
        << pkt.sn
        << pkt.partcount
        << pkt.partindex
        << pkt.payloadsize;
    ds.writeRawData(pkt.payload, static_cast<int>(pkt.payloadsize));
    ds << pkt.crc32 << pkt.endflag;

    return result;
}

template <std::size_t PacketSize>
std::pair<std::string, packet<PacketSize>> deserialize_packet (QByteArray const & data)
{
    using packet_type = packet<PacketSize>;
    using result_type = std::pair<std::string, packet_type>;

    assert(data.size() == packet_type::PACKET_SIZE);

    packet_type pkt;
    QDataStream ds {data};
    ds.setByteOrder(QDataStream::BigEndian);

    ds >> pkt.startflag
        >> pkt.uuid
        >> pkt.sn
        >> pkt.partcount
        >> pkt.partindex
        >> pkt.payloadsize;

    ds.readRawData(pkt.payload, pkt.payloadsize);

    ds >> pkt.crc32 >> pkt.endflag;

    if (pkt.startflag != packet_type::START_FLAG)
        return std::make_pair("bad packet: bad START flag", packet_type{});

    if (pkt.startflag != packet_type::END_FLAG)
        return std::make_pair("bad packet: bad END flag", packet_type{});

    std::int32_t crc32 = pfs::crc32_all_of(0, pkt.uuid
        , pkt.sn, pkt.partcount, pkt.partindex);

    crc32 = pfs::crc32_of_ptr(pkt.payload, packet_type::PAYLOAD_SIZE, crc32);

    if (crc32 != pkt.crc32)
        return std::make_pair("bad CRC32", packet_type{});

    return std::make_pair(std::string{}, std::move(pkt));
}

}}}} // namespace pfs::net::p2p::qt5
