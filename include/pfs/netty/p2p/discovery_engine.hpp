////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2023 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2023.01.17 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "hello_packet.hpp"
#include "universal_id.hpp"
#include "pfs/time_point.hpp"
#include "pfs/i18n.hpp"
#include "pfs/netty/chrono.hpp"
#include "pfs/netty/error.hpp"
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
    static constexpr int MAX_TRANSMIT_INTERVAL_SECONDS = 60;

    using backend_type         = Backend;
    using input_envelope_type  = input_envelope<>;
    using output_envelope_type = output_envelope<>;

public:
//     using receiver_type     = typename Backend::receiver_type;
//     using sender_type       = typename Backend::sender_type;
    using milliseconds_type = std::chrono::milliseconds;
    using timediff_type     = std::chrono::milliseconds;

    enum option_enum: std::int16_t
    {
//           discovery_address     // socket4_addr
//         , listener_port         // std::uint16_t
          transmit_interval     // std::chrono::milliseconds
        , timestamp_error_limit // std::chrono::milliseconds
    };

private:
    struct options {
//         socket4_addr discovery_address;
//         std::uint16_t listener_port;
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
        clock_type::time_point expiration_timepoint;

        // Peers time difference
        timediff_type timediff;
    };

private:
    options _opts;
    universal_id _host_uuid;
    backend_type _backend;
//     receiver_type _receiver;
//     sender_type   _transmitter;
    clock_type::time_point   _transmit_timepoint;
    std::vector<target_item> _targets;

    // Contains discovered peers with additional information
    std::map<universal_id, peer_credentials> _discovered_peers;

public:
    mutable std::function<void (std::string const &)> on_error
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
        static std::chrono::milliseconds const MIN_EXPIRATION_INTERVAL {5000};
        static int const EXPIRATION_INTERVAL_FACTOR = 5;

        if (size < hello_packet::PACKET_SIZE)
            return;

        auto first = data;
        auto last = data + size;

        while (first < last) {
            input_envelope_type in {first, hello_packet::PACKET_SIZE};
            hello_packet packet;

            in.unseal(packet);
            auto success = is_valid(packet);

            // Unconditional increment
            first += hello_packet::PACKET_SIZE;

            if (success) {
                if (packet.crc16 == crc16_of(packet)) {
                    // Ignore self received packets (can happen during
                    // multicast / broadcast transmission)
                    if (packet.uuid != _host_uuid) {
                        auto pos = _discovered_peers.find(packet.uuid);
                        auto expiration_interval = std::chrono::milliseconds(packet.transmit_interval)
                            * EXPIRATION_INTERVAL_FACTOR;

                        if (expiration_interval < MIN_EXPIRATION_INTERVAL)
                            expiration_interval = MIN_EXPIRATION_INTERVAL;

                        auto expiration_timepoint = current_timepoint() + expiration_interval;

                        // Now in milliseconds since epoch in UTC.
                        auto now = std::chrono::duration_cast<milliseconds_type>(
                            pfs::utc_time::now().time_since_epoch());

                        // Calculate time difference.
                        auto timediff = now - milliseconds_type{packet.timestamp};

                        // New peer is discovered
                        if (pos == std::end(_discovered_peers)) {
                            auto res = _discovered_peers.emplace(packet.uuid
                                , peer_credentials {
                                    socket4_addr{saddr.addr, packet.port}
                                    , expiration_timepoint
                                    , timediff
                                });

                            if (!res.second) {
                                throw error {
                                    make_error_code(errc::unexpected_error)
                                    , tr::_("Unable to store discovered peer")
                                };
                            }

                            pos = res.first;
                            peer_discovered(packet.uuid, pos->second.saddr, timediff);
                        } else {
                            // Peer may be modified

                            bool modified = (pos->second.saddr.addr != saddr.addr);
                            modified = modified || (pos->second.saddr.port != packet.port);

                            if (modified) {
                                LOG_TRACE_1("Peer modified (socket address changed): {}: {}=>{}"
                                    , packet.uuid
                                    , to_string(pos->second.saddr)
                                    , to_string(socket4_addr{saddr.addr, packet.port}));

                                peer_expired(packet.uuid, pos->second.saddr);
                            } else {
                                pos->second.expiration_timepoint = expiration_timepoint;

                                auto diffdiff = timediff > pos->second.timediff
                                    ? timediff - pos->second.timediff
                                    : pos->second.timediff - timediff;

                                if (diffdiff < milliseconds_type{0})
                                    diffdiff *= -1;

                                // Notify about peer's timestamp is out of limits.
                                // Store new value.
                                if (diffdiff < _opts.timestamp_error_limit) {
                                    pos->second.timediff = timediff;
                                    peer_timediff(packet.uuid, timediff);
                                }
                            }
                        }
                    }
                } else {
                    on_error(tr::f_("Bad CRC16 for HELO packet received from: {}"
                        , to_string(saddr)));
                }
            } else {
                on_error(tr::f_("Bad HELO packet received from: {}", to_string(saddr)));
            }
        }
    }

    void broadcast_discovery_data ()
    {
        auto now = current_timepoint();
        auto interval_exceeded = (_transmit_timepoint <= now);

        if (interval_exceeded) {
            hello_packet packet;
            packet.uuid = _host_uuid;
//             packet.port = _opts.listener_port;
            packet.transmit_interval = static_cast<std::uint16_t>(_opts.transmit_interval.count());

            for (auto & t: _targets) {
                auto now = std::chrono::duration_cast<milliseconds_type>(
                    pfs::utc_time::now().time_since_epoch());
                packet.timestamp = static_cast<decltype(packet.timestamp)>(now.count());

                packet.counter = ++t.counter;
                packet.crc16 = crc16_of(packet);

                output_envelope_type out;
                out.seal(packet);

                auto data = out.data();
                auto size = data.size();

                PFS__ASSERT(size == hello_packet::PACKET_SIZE, "");

                netty::error err;
                auto bytes_written = _backend.send(t.saddr, data.data(), size, & err);

                if (bytes_written < 0) {
                    on_error(tr::f_("Transmit failure to: {}: {}"
                        , to_string(t.saddr), err.what()));
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
                    , pos->second.expiration_timepoint.time_since_epoch()
                    , now.time_since_epoch());

                peer_expired(pos->first, pos->second.saddr);
                pos = _discovered_peers.erase(pos);
            } else {
                ++pos;
            }
        }
    }

public:
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
    discovery_engine (universal_id host_uuid)
        : _host_uuid(host_uuid)
    {
        _opts.transmit_interval     = milliseconds_type{MAX_TRANSMIT_INTERVAL_SECONDS * 1000};
        _opts.timestamp_error_limit = milliseconds_type{500};

        _backend.data_ready = [this] (socket4_addr saddr, std::vector<char> data) {
            process_discovery_data(saddr, data.data(), data.size());
        };
    }

    ~discovery_engine () = default;

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

                on_error(tr::_("Bad listener port"));
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
                if (msecs.count() > 0
                        && msecs <= std::chrono::seconds{MAX_TRANSMIT_INTERVAL_SECONDS}) {
                    _opts.transmit_interval = msecs;
                    LOG_TRACE_2("Discovery transmit interval: {}"
                        , _opts.transmit_interval);
                    return true;
                }

                on_error(tr::_("Bad transmit interval, must be less or equals to 60 seconds"));
                break;

            case option_enum::timestamp_error_limit:
                if (msecs.count() >= 0) {
                    _opts.timestamp_error_limit = msecs;
                    LOG_TRACE_2("Discovery timestamp error limit: {}"
                        , _opts.timestamp_error_limit);
                    return true;
                }

                on_error(tr::_("Bad timestamp error limit, must be greater or equals to 0 seconds"));
                break;

            default:
                break;
        }

        return false;
    }

    void add_listener (socket4_addr src_saddr, inet4_addr local_addr)
    {
        _backend.add_listener(src_saddr, local_addr);
    }

    void add_target (socket4_addr target_saddr, inet4_addr local_addr)
    {
        _targets.emplace_back(target_item{target_saddr, 0});
        _backend.add_target(target_saddr, local_addr);
    }

    void discover (std::chrono::milliseconds poll_timeout)
    {
        broadcast_discovery_data();
        _backend.poll(poll_timeout);
        check_expiration();
    }
};

}} // namespace netty::p2p
