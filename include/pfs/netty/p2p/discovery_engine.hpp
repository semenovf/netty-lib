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
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"
#include "pfs/time_point.hpp"
#include "pfs/netty/chrono.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/exports.hpp"
#include "pfs/netty/send_result.hpp"
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
    // Maximum and minimum transmit interval in seconds
    static constexpr int MAX_TRANSMIT_INTERVAL_SECONDS = 300;
    static constexpr int MIN_TRANSMIT_INTERVAL_SECONDS =   1;

    using backend_type         = Backend;
    using input_envelope_type  = input_envelope<>;
    using output_envelope_type = output_envelope<>;

public:
    using milliseconds_type = std::chrono::milliseconds;
    using timediff_type     = std::chrono::milliseconds;

    struct options {
        //milliseconds_type transmit_interval {MAX_TRANSMIT_INTERVAL_SECONDS * 1000};
        milliseconds_type timestamp_error_limit {500};

        // Port on which server will accept incoming connections (readers)
        std::uint16_t host_port {0};
    };

private:
    struct target_item {
        socket4_addr saddr;
        std::uint32_t counter;
        milliseconds_type transmit_interval;
        clock_type::time_point transmit_timepoint;
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
    universal_id _host_uuid;
    options      _opts;
    backend_type _backend;
    clock_type::time_point   _nearest_transmit_timepoint;
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
                                      errc::unexpected_error
                                    , tr::_("unable to store discovered peer")
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
                    on_error(tr::f_("bad CRC16 for HELO packet received from: {}"
                        , to_string(saddr)));
                }
            } else {
                on_error(tr::f_("bad HELO packet received from: {}", to_string(saddr)));
            }
        }
    }

    void broadcast_discovery_data ()
    {
        if (_targets.empty())
            return;

        auto now = current_timepoint();
        auto interval_exceeded = (_nearest_transmit_timepoint <= now);

        if (interval_exceeded) {
            hello_packet packet;
            packet.uuid = _host_uuid;
            packet.port = _opts.host_port;

            for (auto & t: _targets) {
                interval_exceeded = (t.transmit_timepoint <= now);

                if (!interval_exceeded)
                    continue;

                auto now = std::chrono::duration_cast<milliseconds_type>(
                    pfs::utc_time::now().time_since_epoch());
                packet.transmit_interval = static_cast<std::uint16_t>(t.transmit_interval.count());
                packet.timestamp = static_cast<decltype(packet.timestamp)>(now.count());

                packet.counter = ++t.counter;
                packet.crc16 = crc16_of(packet);

                output_envelope_type out;
                out.seal(packet);

                auto data = out.data();
                auto size = data.size();

                PFS__ASSERT(size == hello_packet::PACKET_SIZE, "");

                netty::error err;
                auto res = _backend.send(t.saddr, data.data(), size, & err);

                if (res.state != netty::send_status::good) {
                    on_error(tr::f_("Transmit failure to: {}: {}"
                        , to_string(t.saddr), err.what()));
                }

                t.transmit_timepoint = current_timepoint() + t.transmit_interval;
                _nearest_transmit_timepoint = (std::min)(_nearest_transmit_timepoint
                    , t.transmit_timepoint);
            }
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

    void expire_all_peers ()
    {
        for (auto pos = _discovered_peers.begin(); pos != _discovered_peers.end();) {
            peer_expired(pos->first, pos->second.saddr);
            pos = _discovered_peers.erase(pos);
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
    discovery_engine (universal_id host_uuid, options const & opts)
        : _host_uuid(host_uuid)
        , _opts(opts)
    {
        auto bad = false;
        std::string invalid_argument_desc;

        do {
            bad = _opts.timestamp_error_limit < milliseconds_type{0};

            if (bad) {
                invalid_argument_desc = tr::_("discovery timestamp error limit");
                break;
            }

            bad = _opts.host_port < 1024;

            if (bad) {
                invalid_argument_desc = tr::_("host port");
                break;
            }
        } while (false);

        if (bad) {
            error err {
                  errc::invalid_argument
                , invalid_argument_desc
            };

            throw err;
        }

        _backend.data_ready = [this] (socket4_addr saddr, std::vector<char> data) {
            process_discovery_data(saddr, data.data(), data.size());
        };
    }

    ~discovery_engine ()
    {
        expire_all_peers();
    }

    /**
     * Add receiver.
     *
     * @param src_saddr Receiver address (unicast, multicast or broadcast).
     * @param local_addr Local address for multicast or broadcast.
     */
    void add_receiver (socket4_addr src_saddr
        , inet4_addr local_addr = inet4_addr::any_addr_value)
    {
        _backend.add_receiver(src_saddr, local_addr);
    }

    bool has_receivers () const noexcept
    {
        return _backend.has_receivers();
    }

    /**
     * Add target.
     *
     * @param dest_saddr Target address (unicast, multicast or broadcast).
     * @param local_addr Multicast interface.
     */
    void add_target (socket4_addr target_saddr
        , inet4_addr local_addr
        , milliseconds_type transmit_interval
        , error * perr = nullptr)
    {
        auto bad = transmit_interval < milliseconds_type{MIN_TRANSMIT_INTERVAL_SECONDS}
            || transmit_interval > std::chrono::seconds{MAX_TRANSMIT_INTERVAL_SECONDS};

        if (bad) {
            error err {
                  errc::invalid_argument
                , tr::f_("discovery transmit inteval"
                    " must greater or equals to {} and less or equals to {} seconds"
                    , MIN_TRANSMIT_INTERVAL_SECONDS
                    , MAX_TRANSMIT_INTERVAL_SECONDS)
            };

            if (perr) {
                *perr = err;
                return;
            } else {
                throw err;
            }
        }

        // There is no problem if the discovery process starts much later.
        auto transmit_timepoint = current_timepoint() + transmit_interval;
        _nearest_transmit_timepoint = (std::min)(_nearest_transmit_timepoint
            , transmit_timepoint);

        _targets.emplace_back(target_item{target_saddr, 0, transmit_interval, transmit_timepoint});
        _backend.add_target(target_saddr, local_addr);
    }

    void add_target (socket4_addr target_saddr, milliseconds_type transmit_interval
        , error * perr = nullptr)
    {
        add_target(target_saddr, inet4_addr::any_addr_value, transmit_interval, perr);
    }

    bool has_targets () const noexcept
    {
        return _backend.has_targets();
    }

    /**
     * Force peer expiration
     */
    void expire_peer (universal_id uuid)
    {
        auto pos = _discovered_peers.find(uuid);

        if (pos != _discovered_peers.end()) {
            peer_expired(pos->first, pos->second.saddr);
            pos = _discovered_peers.erase(pos);
        }
    }

    /**
     * @return Pair of number of input and output events (this is a result of
     *         call of backend's poll routine).
     */
    int discover (milliseconds_type poll_timeout = milliseconds_type{0}
        , error * perr = nullptr)
    {
        broadcast_discovery_data();
        auto n = _backend.poll(poll_timeout, perr);
        check_expiration();
        return n;
    }
};

template <typename Backend> constexpr int const
discovery_engine<Backend>::MIN_TRANSMIT_INTERVAL_SECONDS;

template <typename Backend> constexpr int const
discovery_engine<Backend>::MAX_TRANSMIT_INTERVAL_SECONDS;

}} // namespace netty::p2p
