
////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.11.19 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
// #include "envelope.hpp"
// #include "hello_packet.hpp"
// #include "packet.hpp"
// #include "timing.hpp"
// #include "trace.hpp"
#include "uuid.hpp"
// #include "pfs/ring_buffer.hpp"
#include "pfs/emitter.hpp"
#include "pfs/net/inet4_addr.hpp"
// #include <array>
// #include <list>
// #include <unordered_map>
// #include <utility>
// #include <vector>
// #include <cassert>

namespace pfs {
namespace net {
namespace p2p {

class controller
{
public: // signals
    pfs::emitter_mt<std::string const &> failure;

    /**
     * Emitted when new writer socket ready (connected).
     */
    emitter_mt<uuid_t, inet4_addr, std::uint16_t> writer_ready;

    /**
     * Emitted when new address accepted by discoverer.
     */
    emitter_mt<uuid_t, inet4_addr, std::uint16_t> rookie_accepted;

    /**
     * Emitted when address expired (update is timed out).
     */
    emitter_mt<uuid_t, inet4_addr, std::uint16_t> peer_expired;

    emitter_mt<uuid_t, std::string> message_received;

    emitter_mt<uuid_t, char const * /*data*/
        , std::streamsize /*len*/, int /*priority*/> send;
};

}}} // namespace pfs::net::p2p
