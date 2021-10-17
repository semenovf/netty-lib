////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.10.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "seqnum.hpp"
#include "serializer.hpp"
#include "seqnum.hpp"
#include "pfs/crc16.hpp"
#include "pfs/uuid.hpp"
#include "pfs/uuid_crc.hpp"
#include <cereal/archives/binary.hpp>

namespace pfs {
namespace net {
namespace p2p {

enum handshake_phase: std::uint8_t {
      syn_phase = 42
    , synack_phase
    , ack_phase
};

struct handshake_packet
{
    static constexpr std::uint8_t START_FLAG   = 0xBE;
    static constexpr std::uint8_t END_FLAG     = 0xED;

    static constexpr std::size_t PACKET_SIZE =
          sizeof(std::uint8_t)  // phase
        + 16                    // uuid
        + sizeof(seqnum_t)      // sn
        + sizeof(std::int16_t); // crc16

    std::uint8_t  startflag {START_FLAG};
    std::uint8_t phase;
    uuid_t uuid;
    seqnum_t sn;
    std::int16_t crc16;
    std::uint8_t  endflag {END_FLAG};
};

constexpr std::size_t handshake_packet::PACKET_SIZE;

inline std::int16_t crc16_of (handshake_packet const & pkt)
{
    auto crc16 = pfs::crc16_all_of(0, pkt.phase, pkt.uuid);
    return crc16;
}

void save (cereal::BinaryOutputArchive & ar, handshake_packet const & pkt)
{
    ar << pkt.startflag
        << pkt.phase
        << pfs::to_network_order(pkt.uuid)
        << pfs::to_network_order(crc16_of(pkt))
        << pkt.endflag;
}

void load (cereal::BinaryInputArchive & ar, handshake_packet & pkt)
{
    ar >> pkt.phase
        >> ntoh_wrapper<decltype(pkt.uuid)>(pkt.uuid)
        >> ntoh_wrapper<decltype(pkt.crc16)>(pkt.crc16);
}

inline std::pair<bool, std::string>
validate (handshake_packet const & pkt)
{
    if (pkt.startflag != handshake_packet::START_FLAG)
        return std::make_pair(false, "invalid start flag");

    if (pkt.endflag != handshake_packet::END_FLAG)
        return std::make_pair(false, "invalid end flag");

    if (crc16_of(pkt) != pkt.crc16)
        return std::make_pair(false, "bad CRC16");

    return std::make_pair(true, std::string{});
}

}}} // namespace pfs::net::p2p

