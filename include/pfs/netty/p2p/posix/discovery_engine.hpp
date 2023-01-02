////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.12.31 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "discovery_socket.hpp"
#include "pfs/time_point.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include "pfs/netty/p2p/envelope.hpp"
#include <chrono>
#include <functional>
#include <map>
#include <vector>

namespace netty {
namespace p2p {
namespace posix {

class discovery_engine
{
public:
    using input_envelope_type  = input_envelope<>;
    using output_envelope_type = output_envelope<>;
    using socket_type = discovery_socket;

// private:
//     void process_discovery_data (socket4_addr saddr, char const * data
//         , std::size_t size);
//
//     void broadcast_discovery_data ();
//     void check_expiration ();

// private:
//     discovery_engine () = delete;
//     discovery_engine (discovery_engine const &) = delete;
//     discovery_engine (discovery_engine &&) = delete;
//     discovery_engine & operator = (discovery_engine const &) = delete;
//     discovery_engine & operator = (discovery_engine &&) = delete;

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

