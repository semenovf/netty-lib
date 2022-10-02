////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019-2022 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2022.09.20 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/netty/chrono.hpp"
#include "pfs/netty/error.hpp"
#include "pfs/netty/p2p/hello_packet.hpp"
#include "pfs/netty/p2p/qt5/discovery_engine.hpp"
#include "pfs/i18n.hpp"
#include "pfs/log.hpp"

namespace netty {
namespace p2p {
namespace qt5 {

static std::chrono::milliseconds const MAX_TRANSMIT_INTERVAL {60 * 1000};
static std::chrono::milliseconds const DEFAULT_TRANSMIT_INTERVAL {  5000};

discovery_engine::discovery_engine (universal_id host_uuid)
    : _host_uuid(host_uuid)
{
    _opts.transmit_interval = DEFAULT_TRANSMIT_INTERVAL;
    _opts.listener_port = 0;

    _receiver.data_ready = [this] (socket4_addr saddr, char const * data
        , std::size_t size) {
        process_discovery_data(saddr, data, size);
    };
}

bool discovery_engine::set_option (option_enum opttype, std::intmax_t value)
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

bool discovery_engine::set_option (option_enum opttype, socket4_addr sa)
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

bool discovery_engine::set_option (option_enum opttype, std::chrono::milliseconds msecs)
{
    switch (opttype) {
        case option_enum::transmit_interval:
            if (msecs.count() > 0 && msecs <= MAX_TRANSMIT_INTERVAL) {
                _opts.transmit_interval = msecs;
                return true;
            }

            log_error(tr::_("Bad transmit interval, must be less or equals"
                " to 60 seconds"));
            break;

        default:
            break;
    }

    return false;
}

void discovery_engine::listen ()
{
    auto now = current_timepoint();

    _last_timepoint = now > _opts.transmit_interval
        ? now - _opts.transmit_interval : now;

    _receiver.bind(_opts.discovery_address);

    LOG_TRACE_2("Discovery listener: {}. Status: {}"
        , _receiver
        , _receiver.state_string());
}

void discovery_engine::loop ()
{
    broadcast_discovery_data();
    _receiver.process_incoming_data();
    check_expiration();
}

void discovery_engine::process_discovery_data (socket4_addr saddr
    , char const * data, std::size_t size)
{
    input_envelope_type in {data, size};
    hello_packet packet;

    in.unseal(packet);
    auto success = is_valid(packet);

    if (success) {
        if (packet.crc16 == crc16_of(packet)) {
            // Ignore self received packets (can happen during
            // multicast / broadcast transmission)
            if (packet.uuid != _host_uuid) {
                auto pos = _discovered_peers.find(packet.uuid);
                auto expiration_timepoint = current_timepoint()
                    + std::chrono::milliseconds(packet.transmit_interval) * 2;

                // New peer is discovered
                if (pos == std::end(_discovered_peers)) {
                    auto res = _discovered_peers.emplace(packet.uuid
                        , peer_credentials{
                              socket4_addr{saddr.addr, packet.port}
                            , expiration_timepoint
                        });

                    if (!res.second) {
                        throw error {
                              make_error_code(errc::unexpected_error)
                            , tr::_("Unable to store discovered peer")
                        };
                    }
                    pos = res.first;
                    peer_discovered(packet.uuid, pos->second.saddr);
                } else {
                    // Peer may be modified

                    bool modified = (pos->second.saddr.addr != saddr.addr);
                    modified = modified || (pos->second.saddr.port != packet.port);

                    if (modified) {
                        peer_expired(packet.uuid, pos->second.saddr);
                        pos->second.saddr = socket4_addr{saddr.addr, packet.port};
                        peer_discovered(packet.uuid, pos->second.saddr);
                    }
                }

                pos->second.expiration_timepoint = expiration_timepoint;
            }
        } else {
            if (log_error) {
                log_error(tr::f_("Bad CRC16 for HELO packet received from: {}:{}"
                    , to_string(saddr.addr), saddr.port));
            }
        }
    } else {
        if (log_error) {
            log_error(tr::f_("Bad HELO packet received from: {}:{}"
                , to_string(saddr.addr), saddr.port));
        }
    }
}

void discovery_engine::broadcast_discovery_data ()
{
    auto now = current_timepoint();
    bool interval_exceeded = false;

    if (_last_timepoint > now) {
        interval_exceeded = true;
    } else {
        interval_exceeded = (_last_timepoint + _opts.transmit_interval) <= now;
    }

    if (interval_exceeded) {
        hello_packet packet;
        packet.uuid = _host_uuid;
        packet.port = _opts.listener_port;
        packet.transmit_interval = static_cast<std::uint16_t>(_opts.transmit_interval.count());
        packet.crc16 = crc16_of(packet);

        output_envelope_type out;
        out.seal(packet);

        auto data = out.data();
        auto size = data.size();

        PFS__ASSERT(size == hello_packet::PACKET_SIZE, "");

        for (auto const & item: _targets) {
            socket4_addr target_saddr {item.addr, item.port};
            auto bytes_written = _transmitter.send(data.data(), size, target_saddr);

            if (bytes_written < 0) {
                if (log_error) {
                    log_error(tr::f_("Transmit failure to: {}: {}"
                        , to_string(target_saddr), _transmitter.error_string()));
                }
            }
        }

        _last_timepoint = current_timepoint();
    }
}

/**
 * Check if the peer is alive
 */
void discovery_engine::check_expiration ()
{
    auto now = current_timepoint();

    for (auto pos = _discovered_peers.begin(); pos != _discovered_peers.end();) {
        if (pos->second.expiration_timepoint < now) {
            LOG_TRACE_2("Discovered peer expired by timeout: {}@{}"
                , pos->first, to_string(pos->second.saddr));

            peer_expired(pos->first, pos->second.saddr);
            pos = _discovered_peers.erase(pos);
        } else {
            ++pos;
        }
    }
}

void discovery_engine::add_target (socket4_addr saddr)
{
    _targets.emplace_back(saddr);

    if (is_multicast(saddr.addr)) {
        _receiver.join_multicast_group(saddr.addr);

        LOG_TRACE_2("Discovery receiver joined into multicast group: {}"
            , to_string(saddr.addr));
    }
}

}}} // namespace netty::p2p::qt5
