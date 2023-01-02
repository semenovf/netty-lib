#pragma once
#include "pfs/netty/p2p/engine.hpp"
#include "pfs/netty/p2p/posix/discovery_engine.hpp"
#include "pfs/netty/p2p/posix/sockets_api.hpp"
#include "pfs/netty/p2p/file_transporter.hpp"

static constexpr std::size_t PACKET_SIZE = netty::p2p::packet::MAX_PACKET_SIZE; // = 1430

using posix_p2p_engine_t = netty::p2p::engine<
      netty::p2p::posix::discovery_engine
    , netty::p2p::posix::sockets_api
    , netty::p2p::file_transporter
    , PACKET_SIZE>;
