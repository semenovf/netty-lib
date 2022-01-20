
////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2021 Vladislav Trifochkin
//
// This file is part of [net-lib](https://github.com/semenovf/net-lib) library.
//
// Changelog:
//      2021.11.19 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "uuid.hpp"
#include "pfs/emitter.hpp"
#include "pfs/netty/inet4_addr.hpp"

namespace netty {
namespace p2p {

class controller
{
public: // signals
    pfs::emitter_mt<std::string const &> failure;

    /**
     * Emitted when new writer socket ready (connected).
     */
    pfs::emitter_mt<uuid_t, inet4_addr, std::uint16_t> writer_ready;

    /**
     * Emitted when new address accepted by discoverer.
     */
    pfs::emitter_mt<uuid_t, inet4_addr, std::uint16_t> rookie_accepted;

    /**
     * Emitted when address expired (update is timed out).
     */
    pfs::emitter_mt<uuid_t, inet4_addr, std::uint16_t> peer_expired;

    pfs::emitter_mt<uuid_t, std::string> message_received;

    pfs::emitter_mt<uuid_t, char const * /*data*/
        , std::streamsize /*len*/, int /*priority*/> send;
};

}} // namespace netty::p2p
