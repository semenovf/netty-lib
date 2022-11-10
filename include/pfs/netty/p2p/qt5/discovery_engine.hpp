////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "udp_socket.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include "pfs/netty/p2p/envelope.hpp"
#include <chrono>
#include <functional>
#include <map>
#include <vector>

namespace netty {
namespace p2p {
namespace qt5 {

class discovery_engine
{
    using input_envelope_type  = input_envelope<>;
    using output_envelope_type = output_envelope<>;

public:
    using socket_type = udp_socket;

    enum option_enum: std::int16_t
    {
          discovery_address // socket4_addr
        , transmit_interval // std::chrono::milliseconds
        , listener_port     // std::uint16_t
    };

private:
    struct options {
        socket4_addr discovery_address;
        std::chrono::milliseconds transmit_interval;
        std::uint16_t listener_port;
    };

    struct target_item {
        socket4_addr saddr;
        std::uint32_t counter;
    };

private:
    options _opts;
    universal_id _host_uuid;
    socket_type  _receiver;
    socket_type  _transmitter;
    std::chrono::milliseconds _transmit_timepoint;
    std::vector<target_item>  _targets;

    struct peer_credentials
    {
        socket4_addr saddr;

        // Expiration timepoint to monitor if the peer is alive
        std::chrono::milliseconds expiration_timepoint;
    };

    // Contains discovered peers with additional information
    std::map<universal_id, peer_credentials> _discovered_peers;

public:
    mutable std::function<void (std::string const &)> log_error
        = [] (std::string const &) {};

    /**
     * Called when peer discovery (hello) packet received.
     * It happens periodically (defined by _transmit_interval on the remote host).
     */
    mutable std::function<void (universal_id, socket4_addr)> peer_discovered
        = [] (universal_id /*peer_uuid*/, socket4_addr /*saddr*/) {};

    /**
     * Called when no discovery packets were received for a specified period or
     * when any of the credential properties have changed.
     * This is an opposite event to `peer_discovered`.
     */
    mutable std::function<void (universal_id, socket4_addr)> peer_expired
        = [] (universal_id /*peer_uuid*/, socket4_addr /*saddr*/) {};

private:
    void process_discovery_data (socket4_addr saddr, char const * data
        , std::size_t size);

    void broadcast_discovery_data ();
    void check_expiration ();

private:
    discovery_engine () = delete;
    discovery_engine (discovery_engine const &) = delete;
    discovery_engine (discovery_engine &&) = delete;
    discovery_engine & operator = (discovery_engine const &) = delete;
    discovery_engine & operator = (discovery_engine &&) = delete;

public:
    /**
     * @param host_uuid Host unique identifier.
     * @param transmit_interval Discovery packet sending interval.
     * @param server_port The port on which the server will accept remote
     *        connections.
     */
    NETTY__EXPORT discovery_engine (universal_id host_uuid);

    NETTY__EXPORT ~discovery_engine ();

    /**
     * Sets boolean or integer type options.
     *
     * @return @c true on success, or @c false if option is unsuitable or
     *         value is bad.
     */
    NETTY__EXPORT bool set_option (option_enum opttype, std::intmax_t value);

    /**
     * Sets socket address type options.
     */
    NETTY__EXPORT bool set_option (option_enum opttype, socket4_addr sa);

    /**
     * Sets chrono type option.
     */
    NETTY__EXPORT bool set_option (option_enum opttype, std::chrono::milliseconds msecs);

    /**
     * Initiates discovery procedure.
     *
     * @param saddr The address to which the listener socket is bound.
     */
    NETTY__EXPORT void listen ();

    NETTY__EXPORT void add_target (socket4_addr saddr);
    NETTY__EXPORT void loop ();
};

}}} // namespace netty::p2p::qt5
