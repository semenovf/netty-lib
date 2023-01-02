////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.12.31 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "hello_packet.hpp"
#include "universal_id.hpp"
#include "pfs/time_point.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/chrono.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/socket4_addr.hpp"
#include "pfs/netty/p2p/envelope.hpp"
#include <chrono>
#include <functional>
#include <map>
#include <vector>

namespace netty {
namespace p2p {

template <typename Backend>
class discovery_engine
{
    // Maximum transmit interval in seconds
    static constexpr int MAX_TRANSMIT_INTERVAL = 60;

    using input_envelope_type  = typename Backend::input_envelope_type;
    using output_envelope_type = typename Backend::output_envelope_type;

public:
    using socket_type       = typename Backend::socket_type;
    using milliseconds_type = std::chrono::milliseconds;
    using timediff_type     = std::chrono::milliseconds;

    enum option_enum: std::int16_t
    {
          discovery_address     // socket4_addr
        , listener_port         // std::uint16_t
        , transmit_interval     // std::chrono::milliseconds
        , timestamp_error_limit // std::chrono::milliseconds
    };

private:
    struct options {
        socket4_addr discovery_address;
        std::uint16_t listener_port;
        milliseconds_type transmit_interval;
        milliseconds_type timestamp_error_limit;
    };

    struct target_item {
        socket4_addr saddr;
        std::uint32_t counter;
    };

    struct peer_credentials
    {
        socket4_addr saddr;

        // Expiration timepoint to monitor if the peer is alive
        milliseconds_type expiration_timepoint;

        // Peers time difference
        timediff_type timediff;
    };

private:
    options _opts;
    universal_id _host_uuid;
    socket_type  _receiver;
    socket_type  _transmitter;
    milliseconds_type _transmit_timepoint;
    std::vector<target_item> _targets;

    // Contains discovered peers with additional information
    std::map<universal_id, peer_credentials> _discovered_peers;

public:
    mutable std::function<void (std::string const &)> log_error
        = [] (std::string const &) {};

    /**
     * Called when peer discovery (hello) packet received.
     * It happens periodically (defined by _transmit_interval on the remote host).
     */
    mutable std::function<void (universal_id, socket4_addr, timediff_type const &)>
        peer_discovered = [] (universal_id, socket4_addr, timediff_type const &) {};

    /**
     * Called when the time difference has changed significantly.
     */
    mutable std::function<void (universal_id, timediff_type const &)>
        peer_timediff = [] (universal_id, timediff_type const &) {};

    /**
     * Called when no discovery packets were received for a specified period or
     * when any of the credential properties have changed.
     * This is an opposite event to `peer_discovered`.
     */
    mutable std::function<void (universal_id, socket4_addr)> peer_expired
        = [] (universal_id /*peer_uuid*/, socket4_addr /*saddr*/) {};

private:
    void process_discovery_data (socket4_addr saddr
        , char const * data, std::size_t size)
    {
        auto now = current_timepoint();
        auto interval_exceeded = (_transmit_timepoint <= now);

        if (interval_exceeded) {
            hello_packet packet;
            packet.uuid = _host_uuid;
            packet.port = _opts.listener_port;
            packet.transmit_interval = static_cast<std::uint16_t>(_opts.transmit_interval.count());

            for (auto & item: _targets) {
                packet.counter = ++item.counter;
                packet.crc16 = crc16_of(packet);

                output_envelope_type out;
                out.seal(packet);

                auto data = out.data();
                auto size = data.size();

                PFS__ASSERT(size == hello_packet::PACKET_SIZE, "");

                socket4_addr target_saddr {item.saddr.addr, item.saddr.port};
                auto bytes_written = _transmitter.send(data.data(), size, target_saddr);

                if (bytes_written < 0) {
                    log_error(tr::f_("Transmit failure to: {}: {}"
                        , to_string(target_saddr), _transmitter.error_string()));
                }
            }

            _transmit_timepoint = current_timepoint() + _opts.transmit_interval;
        }
    }

    void broadcast_discovery_data ()
    {
        auto now = current_timepoint();
        auto interval_exceeded = (_transmit_timepoint <= now);

        if (interval_exceeded) {
            hello_packet packet;
            packet.uuid = _host_uuid;
            packet.port = _opts.listener_port;
            packet.transmit_interval = static_cast<std::uint16_t>(_opts.transmit_interval.count());

            for (auto & item: _targets) {
                packet.counter = ++item.counter;
                packet.crc16 = crc16_of(packet);

                output_envelope_type out;
                out.seal(packet);

                auto data = out.data();
                auto size = data.size();

                PFS__ASSERT(size == hello_packet::PACKET_SIZE, "");

                socket4_addr target_saddr {item.saddr.addr, item.saddr.port};
                auto bytes_written = _transmitter.send(data.data(), size, target_saddr);

                if (bytes_written < 0) {
                    log_error(tr::f_("Transmit failure to: {}: {}"
                        , to_string(target_saddr), _transmitter.error_string()));
                }
            }

            _transmit_timepoint = current_timepoint() + _opts.transmit_interval;
        }
    }

    void check_expiration ()
    {
        auto now = current_timepoint();

        for (auto pos = _discovered_peers.begin(); pos != _discovered_peers.end();) {
            if (pos->second.expiration_timepoint < now) {
                LOG_TRACE_2("Discovered peer expired by timeout: {}@{}: {} < {}"
                    , pos->first, to_string(pos->second.saddr)
                    , pos->second.expiration_timepoint
                    , now);

                peer_expired(pos->first, pos->second.saddr);
                pos = _discovered_peers.erase(pos);
            } else {
                ++pos;
            }
        }
    }

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
    bool set_option (option_enum opttype, std::intmax_t value)
    {
        switch (opttype) {
            case option_enum::listener_port:
                if (value >= 0 && value <= (std::numeric_limits<std::uint16_t>::max)()) {
                    _opts.listener_port = static_cast<std::uint16_t>(value);
                    return true;
                }

                log_error(tr::_("Bad listener port"));
                break;

            default:
                break;
        }

        return false;
    }

    /**
     * Sets socket address type options.
     */
    bool set_option (option_enum opttype, socket4_addr sa)
    {
        switch (opttype) {
            case option_enum::discovery_address:
                _opts.discovery_address = sa;
                return true;
            default:
                break;
        }

        return false;
    }

    /**
     * Sets chrono type option.
     */
    bool set_option (option_enum opttype, std::chrono::milliseconds msecs)
    {
        switch (opttype) {
            case option_enum::transmit_interval:
                if (msecs.count() > 0 && msecs <= std::chrono::seconds{MAX_TRANSMIT_INTERVAL}) {
                    _opts.transmit_interval = msecs;
                    LOG_TRACE_2("Discovery transmit interval: {}"
                        , _opts.transmit_interval);
                    return true;
                }

                log_error(tr::_("Bad transmit interval, must be less or equals to 60 seconds"));
                break;

            case option_enum::timestamp_error_limit:
                if (msecs.count() >= 0) {
                    _opts.timestamp_error_limit = msecs;
                    LOG_TRACE_2("Discovery timestamp error limit: {}"
                        , _opts.timestamp_error_limit);
                    return true;
                }

                log_error(tr::_("Bad timestamp error limit, must be greater or equals to 0 seconds"));
                break;

            default:
                break;
        }

        return false;
    }

    /**
     * Initiates discovery procedure.
     *
     * @param saddr The address to which the listener socket is bound.
     */
    void listen ()
    {
        auto now = current_timepoint();

        _transmit_timepoint = now + _opts.transmit_interval;

        _receiver.bind(_opts.discovery_address);

        LOG_TRACE_2("Discovery listener: {}. Status: {}"
            , _receiver
            , _receiver.state_string());
    }

    NETTY__EXPORT void add_target (socket4_addr saddr);

    void loop ()
    {
        broadcast_discovery_data();
        _receiver.process_incoming_data();
        check_expiration();
    }
};

}} // namespace netty::p2p
