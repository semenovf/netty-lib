////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2024 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2024.05.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "packet.hpp"
#include "peer_id.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/host4_addr.hpp"
#include <functional>
#include <string>
#include <vector>

namespace netty {
namespace p2p {

struct delivery_functional_callbacks
{
    /**
     * Called when unrecoverable error occurred - engine became disfunctional.
     * It must be restarted.
     */
    mutable std::function<void (error const & err)> on_failure = [] (error const &) {};

    mutable std::function<void (std::string const &)> on_error = [] (std::string const &) {};
    mutable std::function<void (std::string const &)> on_warn = [] (std::string const &) {};

    /**
     * Called when need to force peer expiration by discovery manager (using expire_peer method).
     */
    mutable std::function<void (peer_id)> defere_expire_peer;

    /**
     * Called when new writer socket ready (connected).
     */
    mutable std::function<void (host4_addr)> writer_ready = [] (host4_addr) {};

    /**
     * Called when writer socket closed/disconnected.
     */
    mutable std::function<void (host4_addr)> writer_closed = [] (host4_addr) {};

    /**
     * Called when new reader socket ready (handshaked).
     */
    mutable std::function<void (host4_addr)> reader_ready = [] (host4_addr) {};

    /**
     * Called when reader socket closed/disconnected.
     */
    mutable std::function<void (host4_addr)> reader_closed = [] (host4_addr) {};

    /**
     * Called when channel (reader and writer available) established.
     */
    mutable std::function<void (host4_addr)> channel_established = [] (host4_addr) {};

    /**
     * Called when channel closed.
     */
    mutable std::function<void (peer_id)> channel_closed = [] (peer_id) {};

    /**
     * Data received.
     */
    mutable std::function<void (peer_id, std::vector<char>)> data_received
        = [] (peer_id, std::vector<char>) {};

    /**
     * Called when any file data received. These data must be passed to the file transporter
     */
    mutable std::function<void (peer_id, packet_type_enum, std::vector<char>)> file_data_received
        = [] (peer_id, packet_type_enum packettype, std::vector<char>) {};

    /**
     * Called to request new file chunks for sending.
     */
    mutable std::function<void (peer_id, universal_id)> request_file_chunk
        = [] (peer_id addressee, universal_id fileid) {};
};

}} // namespace netty::p2p
