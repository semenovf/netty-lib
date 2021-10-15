////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.09.13 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pfs/uuid.hpp"

namespace pfs {
namespace net {
namespace p2p {

struct hello_packet
{
    static constexpr std::size_t PACKET_SIZE = 4 + 16 + 2 + 4;

    char greeting[4] = {'H', 'E', 'L', 'O'};
    uuid_t uuid;
    std::uint16_t port {0};
    std::int32_t crc32;
};

constexpr std::size_t hello_packet::PACKET_SIZE;

}}} // namespace pfs::net::p2p
